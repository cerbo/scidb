#!/bin/bash
# Test of TLS encrypted SciDB authentication

echo
echo This test assumes default root/Paradigm4 user/password.
echo

host=localhost
port=8089
td=$(mktemp -d)

function fail {
  echo "FAIL"
  rm -rf $td
  kill -9 %1
  exit 1
}

mkdir -p $td/wwwroot
# Add a tls cert
openssl req -new -newkey rsa:4096 -days 3650 -nodes -x509 -subj "/C=US/ST=MA/L=Waltham/O=Paradigm4/CN=$(hostname)" -keyout $td/ssl_cert.pem 2>/dev/null >> $td/ssl_cert.pem
./shim -p ${port}s -r $td/wwwroot  -f &
sleep 1

id=$(curl -f -s -k "https://${host}:${port}/new_session" | sed -e "s/.*//")
curl -f -k "https://${host}:${port}/execute_query?id=${id}&query=list('functions')&save=dcsv&user=root&password=eUCUk3B57IVO9ZfJB6CIEHl%2F0lxrWg%2F7PV8KytUNY6kPLhTX2db48GHGHoizKyH%2BuGkCfNTYZrJgKzjWOhjuvg%3D%3D" || fail
curl -f -k "https://${host}:${port}/read_lines?id=${id}&n=0" || fail
curl -f -k "https://${host}:${port}/release_session?id=${id}" || fail
echo "OK"

rm -rf $td
kill -9 %1 >/dev/null 2>&1
