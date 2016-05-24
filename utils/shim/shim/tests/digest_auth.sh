#!/bin/bash

host=localhost
port=8088
td=$(mktemp -d)

function fail {
  echo "FAIL"
  rm -rf $td
  kill -9 %1
  exit 1
}

mkdir -p $td/wwwroot
echo "homer:elmo" > $td/wwwroot/.htpasswd
./shim -p $port -r $td/wwwroot  -f &
sleep 1

id=$(curl -f -s --digest --user homer:elmo "http://${host}:${port}/new_session" | sed -e "s/.*//")
curl -f -s  --digest --user homer:elmo "http://${host}:${port}/execute_query?id=${id}&query=list('functions')&save=dcsv" >/dev/null || fail
curl -f -s --digest --user homer:elmo "http://${host}:${port}/read_lines?id=${id}&n=0" >/dev/null || fail
curl -f -s --digest --user homer:elmo "http://${host}:${port}/release_session?id=${id}" >/dev/null || fail
echo "OK"

rm -rf $td
kill -9 %1 >/dev/null 2>&1
