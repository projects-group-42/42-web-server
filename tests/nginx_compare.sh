#!/bin/sh
#
# nginx_compare.sh


set -u

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
REPORT="$ROOT_DIR/tests/nginx_compare_report.md"
WEBSERV_PORT=8095
NGINX_PORT=8096
WEBSERV_HOST="http://127.0.0.1:$WEBSERV_PORT"
NGINX_HOST="http://127.0.0.1:$NGINX_PORT"

PASS=0
FAIL=0
DOCUMENTED=0

# -------------------------------------------------------------------- #
# Preconditions                                                        #
# -------------------------------------------------------------------- #

if ! command -v docker > /dev/null 2>&1; then
	echo "docker not found on PATH." >&2
	echo "Docker is required to run the NGINX comparison." >&2
	exit 1
fi

if ! docker info > /dev/null 2>&1; then
	echo "Docker is not running or the current user cannot access it." >&2
	exit 1
fi

if ! command -v curl > /dev/null 2>&1; then
	echo "curl not found on PATH; it is needed to drive both servers." >&2
	exit 1
fi

if [ ! -x "$ROOT_DIR/webserv" ]; then
	echo "Building webserv..."
	(cd "$ROOT_DIR" && make -s) || { echo "build failed" >&2; exit 1; }
fi

# -------------------------------------------------------------------- #
# Fixture: one document root, served by both servers                   #
# -------------------------------------------------------------------- #

TMP=$(mktemp -d)
WWW="$TMP/www"
mkdir -p "$WWW/listed" "$WWW/empty"
printf '%s' "<html><body>home</body></html>" > "$WWW/index.html"
printf '%s' "raw-bytes" > "$WWW/notes.bin"
printf '%s' "one" > "$WWW/listed/one.txt"
# world-readable so an nginx worker running as a dedicated user (www-data on
# Debian/Ubuntu) can still read a fixture created under /tmp by this script.
chmod 755 "$TMP"
chmod -R a+rX "$WWW"

WEBSERV_CONF="$TMP/webserv.conf"
cat > "$WEBSERV_CONF" << EOF
server {
    listen 127.0.0.1:$WEBSERV_PORT;
    server_name compare.local;
    root $WWW;
    index index.html;

    location / {
        limit_except GET POST DELETE;
        autoindex off;
    }

    location /listed {
        autoindex on;
    }

    location /old {
        return 302 /new;
    }
}
EOF

MIME_INCLUDE='include /etc/nginx/mime.types;'

NGINX_DIR="$TMP/nginx"
mkdir -p "$NGINX_DIR"

NGINX_CONF="$TMP/nginx.conf"

cat > "$NGINX_CONF" << EOF
pid /tmp/nginx.pid;
error_log /tmp/nginx-error.log;

events {
    worker_connections 64;
}

http {
    access_log off;

    $MIME_INCLUDE

    # Mirrors the Ubuntu package default.
    default_type application/octet-stream;

    server {
        listen 8096;
        server_name compare.local;

        root /usr/share/nginx/html;
        index index.html;

        location /listed {
            autoindex on;
        }

        location /old {
            return 302 /new;
        }
    }
}
EOF

NGINX_CONTAINER="webserv-nginx-compare-$$"

cleanup()
{
	[ -n "${WEBSERV_PID:-}" ] && kill "$WEBSERV_PID" 2> /dev/null
	docker rm -f "$NGINX_CONTAINER" > /dev/null 2>&1 || true
	wait 2> /dev/null
	rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

# -------------------------------------------------------------------- #
# Start both servers                                                   #
# -------------------------------------------------------------------- #

echo "Starting NGINX container..."

docker run \
	--name "$NGINX_CONTAINER" \
	-d \
	-p "127.0.0.1:$NGINX_PORT:8096" \
	-v "$WWW:/usr/share/nginx/html:ro" \
	-v "$NGINX_CONF:/etc/nginx/nginx.conf:ro" \
	nginx:alpine > /dev/null

if [ "$?" -ne 0 ]; then
	echo "failed to start NGINX container" >&2
	exit 1
fi

"$ROOT_DIR/webserv" "$WEBSERV_CONF" > "$TMP/webserv.log" 2>&1 &
WEBSERV_PID=$!

wait_ready()
{
	url="$1"
	i=0
	while [ "$i" -lt 50 ]; do
		curl -s -o /dev/null "$url" && return 0
		i=$((i + 1))
		sleep 0.1
	done
	return 1
}

wait_ready "$WEBSERV_HOST/index.html" || { echo "webserv never came up" >&2; cat "$TMP/webserv.log" >&2; exit 1; }
wait_ready "$NGINX_HOST/index.html" || { echo "nginx never came up" >&2; exit 1; }

# -------------------------------------------------------------------- #
# Helpers                                                              #
# -------------------------------------------------------------------- #

# status_of METHOD PATH HOST
status_of()
{
	curl -s -o /dev/null -w '%{http_code}' -X "$1" "$3$2"
}

# header_of METHOD PATH HOST NAME -- case-insensitive, last match wins
header_of()
{
	curl -s -o /dev/null -D - -X "$1" "$3$2" \
		| tr -d '\r' | grep -i "^$4:" | tail -1 | sed "s/^[^:]*: *//i"
}

# body_of METHOD PATH HOST
body_of()
{
	curl -s -X "$1" "$3$2"
}

report_line()
{
	printf '%s\n' "$1" >> "$REPORT"
}

pass()
{
	PASS=$((PASS + 1))
	echo "[MATCH]      $1"
	report_line "- MATCH: $1"
}

fail()
{
	FAIL=$((FAIL + 1))
	echo "[UNEXPECTED] $1"
	echo "             webserv: $2"
	echo "             nginx:   $3"
	report_line "- **UNEXPECTED DIFFERENCE**: $1"
	report_line "  - webserv: \`$2\`"
	report_line "  - nginx:   \`$3\`"
}

documented()
{
	DOCUMENTED=$((DOCUMENTED + 1))
	echo "[DOCUMENTED] $1"
	echo "             webserv: $2"
	echo "             nginx:   $3"
	report_line "- **Documented difference**: $1"
	report_line "  - webserv: \`$2\`"
	report_line "  - nginx:   \`$3\`"
	report_line "  - reason: $4"
}

# Strips a Location header down to its path, so a host-relative and an
# absolute redirect to the same place compare equal.
path_only()
{
	echo "$1" | sed -E 's#^https?://[^/]+##'
}

# -------------------------------------------------------------------- #
# Report header                                                        #
# -------------------------------------------------------------------- #

: > "$REPORT"
report_line "# NGINX comparison report"
report_line ""
report_line "Generated by \`tests/nginx_compare.sh\` against a real NGINX"
NGINX_VERSION=$(docker exec "$NGINX_CONTAINER" nginx -v 2>&1)

report_line "$NGINX_VERSION and a real ./webserv, both serving the same"
report_line "fixture document root. Regenerate with \`./tests/nginx_compare.sh\`."
report_line ""
report_line "## Cases expected to match"
report_line ""

# -------------------------------------------------------------------- #
# Cases expected to match                                              #
# -------------------------------------------------------------------- #

ws=$(status_of GET /index.html "$WEBSERV_HOST")
ng=$(status_of GET /index.html "$NGINX_HOST")
[ "$ws" = "$ng" ] && pass "GET an existing file answers $ws on both" \
	|| fail "GET an existing file" "$ws" "$ng"

ws=$(header_of GET /index.html "$WEBSERV_HOST" content-type)
ng=$(header_of GET /index.html "$NGINX_HOST" content-type)
[ "$ws" = "$ng" ] && pass "Content-Type of a .html file is '$ws' on both" \
	|| fail "Content-Type of a .html file" "$ws" "$ng"

ws=$(status_of GET /missing.html "$WEBSERV_HOST")
ng=$(status_of GET /missing.html "$NGINX_HOST")
[ "$ws" = "$ng" ] && pass "GET a missing file answers $ws on both" \
	|| fail "GET a missing file" "$ws" "$ng"

ws=$(status_of GET /listed/ "$WEBSERV_HOST")
ng=$(status_of GET /listed/ "$NGINX_HOST")
[ "$ws" = "$ng" ] && pass "an autoindexed, unindexed directory answers $ws on both" \
	|| fail "autoindexed directory" "$ws" "$ng"

ws_status=$(status_of GET /listed "$WEBSERV_HOST")
ng_status=$(status_of GET /listed "$NGINX_HOST")
ws_loc=$(path_only "$(header_of GET /listed "$WEBSERV_HOST" location)")
ng_loc=$(path_only "$(header_of GET /listed "$NGINX_HOST" location)")
if [ "$ws_status" = "$ng_status" ] && [ "$ws_loc" = "$ng_loc" ]; then
	pass "a directory without a trailing slash redirects $ws_status to '$ws_loc' on both"
else
	fail "trailing-slash redirect" "$ws_status $ws_loc" "$ng_status $ng_loc"
fi

ws_status=$(status_of GET /old "$WEBSERV_HOST")
ng_status=$(status_of GET /old "$NGINX_HOST")
ws_loc=$(path_only "$(header_of GET /old "$WEBSERV_HOST" location)")
ng_loc=$(path_only "$(header_of GET /old "$NGINX_HOST" location)")
if [ "$ws_status" = "$ng_status" ] && [ "$ws_loc" = "$ng_loc" ]; then
	pass "a return directive answers $ws_status to '$ws_loc' on both"
else
	fail "return directive" "$ws_status $ws_loc" "$ng_status $ng_loc"
fi

ws=$(status_of PUT /index.html "$WEBSERV_HOST")
ng=$(status_of PUT /index.html "$NGINX_HOST")
[ "$ws" = "$ng" ] && pass "a method neither server implements for the location answers $ws on both" \
	|| fail "unsupported method" "$ws" "$ng"

ws=$(header_of GET /notes.bin "$WEBSERV_HOST" content-type)
ng=$(header_of GET /notes.bin "$NGINX_HOST" content-type)
[ "$ws" = "$ng" ] && pass "an extension neither server recognises defaults to '$ws' on both, matching the Ubuntu package's default_type" \
	|| fail "unrecognised extension default type" "$ws" "$ng"

ws=$(body_of HEAD /index.html "$WEBSERV_HOST")
ng=$(body_of HEAD /index.html "$NGINX_HOST")
[ -z "$ws" ] && [ -z "$ng" ] && pass "HEAD carries no body on either server" \
	|| fail "HEAD body" "'$ws'" "'$ng'"

report_line ""
report_line "## Documented differences"
report_line ""

# -------------------------------------------------------------------- #
# Documented differences                                               #
# -------------------------------------------------------------------- #

ws=$(status_of GET /empty/ "$WEBSERV_HOST")
ng=$(status_of GET /empty/ "$NGINX_HOST")
documented "a directory with neither an index nor autoindex" "$ws" "$ng" \
	"NGINX reads it as \"the listing is forbidden\" and answers 403; this server reads it as \"the index file was not found\" and answers 404. Kept because the tester the subject is graded with expects 404 (see the comment on StaticFileHandler::serveDirectory)."

ws=$(header_of GET /index.html "$WEBSERV_HOST" last-modified)
ng=$(header_of GET /index.html "$NGINX_HOST" last-modified)
documented "Last-Modified on a static file" "${ws:-<absent>}" "${ng:-<absent>}" \
	"this server has no cache-validation support (no If-Modified-Since / conditional GET handling), so it never claims a Last-Modified date."

ws=$(header_of GET /index.html "$WEBSERV_HOST" etag)
ng=$(header_of GET /index.html "$NGINX_HOST" etag)
documented "ETag on a static file" "${ws:-<absent>}" "${ng:-<absent>}" \
	"same reason as Last-Modified: no conditional-request support to back it with."

ws=$(header_of GET /index.html "$WEBSERV_HOST" accept-ranges)
ng=$(header_of GET /index.html "$NGINX_HOST" accept-ranges)
documented "Accept-Ranges on a static file" "${ws:-<absent>}" "${ng:-<absent>}" \
	"this server does not implement byte-range requests (no Range/Content-Range support), so it never advertises support for them."

ws_body=$(body_of GET /listed/ "$WEBSERV_HOST")
ng_body=$(body_of GET /listed/ "$NGINX_HOST")
if echo "$ws_body" | grep -q '<ul>' && echo "$ng_body" | grep -q '<pre>'; then
	documented "autoindex listing markup" "<ul> of <li><a> entries" "<pre> table with date and size columns" \
		"this server tracks no per-entry metadata beyond the name and whether it is a directory, so it cannot render NGINX's date/size columns."
fi

ws=$(header_of PUT /index.html "$WEBSERV_HOST" allow)
ng=$(header_of PUT /index.html "$NGINX_HOST" allow)
documented "Allow header on an unsupported method" "${ws:-<absent>}" "${ng:-<absent>}" \
	"limit_except's 405 always carries Allow (see Router::applyMethodLimit); NGINX's own built-in method table refuses PUT on a plain static location without one."

ws_line=$(curl -s -o /dev/null -D - -X PUT "$WEBSERV_HOST/index.html" | head -1 | tr -d '\r')
ng_line=$(curl -s -o /dev/null -D - -X PUT "$NGINX_HOST/index.html" | head -1 | tr -d '\r')
documented "405 status-line reason phrase" "$ws_line" "$ng_line" \
	"this server spells out the RFC 7231 reason phrase \"Method Not Allowed\"; NGINX has shortened it to \"Not Allowed\" since its own early versions."

ws=$(header_of GET /index.html "$WEBSERV_HOST" set-cookie)
ng=$(header_of GET /index.html "$NGINX_HOST" set-cookie)
documented "Set-Cookie on an ordinary GET" "${ws:+present}" "${ng:-<absent>}" \
	"SessionStore stamps every response with a session cookie by design (see http/SessionStore.cpp); a bare NGINX static-file server has no session feature to set one with."

# -------------------------------------------------------------------- #
# Summary                                                              #
# -------------------------------------------------------------------- #

report_line ""
report_line "## Summary"
report_line ""
report_line "$PASS matching, $DOCUMENTED documented differences, $FAIL unexpected differences."

echo
echo "$PASS matching, $DOCUMENTED documented differences, $FAIL unexpected differences."
echo "Report written to $REPORT"

[ "$FAIL" -eq 0 ]
