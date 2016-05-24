#!/bin/bash
# Test of TLS encrypted digest authentication

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
# Add the digest user name and password
echo "homer:elmo" > $td/wwwroot/.htpasswd
# Add a tls cert
openssl req -new -newkey rsa:4096 -days 3650 -nodes -x509 -subj "/C=US/ST=MA/L=Waltham/O=Paradigm4/CN=$(hostname)" -keyout $td/ssl_cert.pem 2>/dev/null >> $td/ssl_cert.pem
./shim -p ${port}s -r $td/wwwroot  -f &
sleep 1

id=$(curl -f -s -k --digest --user homer:elmo "https://${host}:${port}/new_session" | sed -e "s/.*//")
curl -f -s -k --digest --user homer:elmo "https://${host}:${port}/execute_query?id=${id}&query=list('functions')&save=dcsv" >/dev/null || fail
curl -f -s -k --digest --user homer:elmo "https://${host}:${port}/read_lines?id=${id}&n=0" >/dev/null || fail
curl -f -s -k --digest --user homer:elmo "https://${host}:${port}/release_session?id=${id}" >/dev/null || fail
echo "OK"

rm -rf $td
kill -9 %1 >/dev/null 2>&1
