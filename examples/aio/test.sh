#!/bin/bash

rm -rf test.out

iquery -aq "
project(
 apply(
  op_count(list('instances')), 
  test, iif(count<3, 
             string(throw('This test needs more instances to run')), 
             'Instance Count OK')
 ), 
 test
)" > test.out 2>&1

iquery -anq "remove(zero_to_255)"                > /dev/null 2>&1
iquery -anq "remove(minus_128_to_127)"           > /dev/null 2>&1
iquery -anq "remove(zero_to_65536)"              > /dev/null 2>&1
iquery -anq "remove(minus_32768_to_32767)"       > /dev/null 2>&1
iquery -anq "remove(big_n_wild)"                 > /dev/null 2>&1

iquery -anq "store( build( <val:string> [x=0:65535999,100000,0],  string(x % 256) ), zero_to_255 )"                       > /dev/null 2>&1
iquery -anq "store( build( <val:string> [x=0:65535999,100000,0],  ' ' + string(x % 256 - 128) + ' ' ), minus_128_to_127 )"            > /dev/null 2>&1
iquery -anq "store( build( <val:string> [x=0:65535999,100000,0],  string(x % 65536) + ' ' ), zero_to_65536 )"                   > /dev/null 2>&1
iquery -anq "store( build( <val:string> [x=0:65535999,100000,0],  ' ' + string(x % 65536 - 32768) ), minus_32768_to_32767 )"    > /dev/null 2>&1
iquery -anq "
store(
 build( <val:string null> [x=0:59999999, 100000, 0],
   iif( x % 6 = 0, string(x / 6 * 100) + ' ',
   iif( x % 6 = 1, ' ' + string(uint64(x) * 200000000000),
   iif( x % 6 = 2, '   ' + string(double(x) / 123.4) + ' ', 
   iif( x % 6 = 3, string(x) + '_abc',
   iif( x % 6 = 4, iif( x % 4 = 0, ' true', 'FalSe '),
                   string(null))))))
 ),
 big_n_wild
)"                                                                                                                        > /dev/null 2>&1

iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('127',  int8(null)))"                                           >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('128',  int8(null)))"                                           >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-128', int8(null)))"                                           >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-129', int8(null)))"                                           >> test.out

iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('255', uint8(null)))"                                           >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('256', uint8(null)))"                                           >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('0',   uint8(null)))"                                           >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-1',  uint8(null)))"                                           >> test.out

iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('32767',  int16(null)))"                                        >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('32768',  int16(null)))"                                        >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-32768', int16(null)))"                                        >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-32769', int16(null)))"                                        >> test.out

iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('65535',  uint16(null)))"                                       >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('65536',  uint16(null)))"                                       >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('0',      uint16(null)))"                                       >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-1',     uint16(null)))"                                       >> test.out

iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('2147483647', int32(null)))"                                    >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('2147483648', int32(null)))"                                    >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-2147483648', int32(null)))"                                   >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-2147483649', int32(null)))"                                   >> test.out

iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('4294967295', uint32(null)))"                                   >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('4294967296', uint32(null)))"                                   >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('0',          uint32(null)))"                                   >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-1',         uint32(null)))"                                   >> test.out

iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('4294967295', uint32(null)))"                                   >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('4294967296', uint32(null)))"                                   >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('0',          uint32(null)))"                                   >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-1',         uint32(null)))"                                   >> test.out

iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('9223372036854775807',  int64(null)))"                          >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('9223372036854775808',  int64(null)))"                          >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-9223372036854775808', int64(null)))"                          >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-9223372036854775809', int64(null)))"                          >> test.out

iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('18446744073709551615', uint64(null)))"                         >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('18446744073709551616', uint64(null)))"                         >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('0', uint64(null)))"                                            >> test.out
iquery -aq "build(<val:double null> [x=0:0,1,0], dcast('-1', uint64(null)))"                                           >> test.out

time iquery -aq "aggregate(apply(zero_to_255, v2, dcast(val, uint8(null))), avg(v2), count(v2), count(*))"             >> test.out
time iquery -aq "aggregate(apply(zero_to_255, v2, dcast(val, int8(null))), avg(v2), count(v2), count(*))"              >> test.out
time iquery -aq "aggregate(apply(minus_128_to_127, v2, dcast(val, uint8(null))), avg(v2), count(v2), count(*))"        >> test.out
time iquery -aq "aggregate(apply(minus_128_to_127, v2, dcast(val, int8(null))), avg(v2), count(v2), count(*))"         >> test.out

time iquery -aq "aggregate(apply(zero_to_65536, v2, dcast(val, uint16(null))), avg(v2), count(v2), count(*))"          >> test.out
time iquery -aq "aggregate(apply(zero_to_65536, v2, dcast(val, int16(null))), avg(v2), count(v2), count(*))"           >> test.out
time iquery -aq "aggregate(apply(minus_32768_to_32767, v2, dcast(val, uint16(null))), avg(v2), count(v2), count(*))"   >> test.out
time iquery -aq "aggregate(apply(minus_32768_to_32767, v2, dcast(val, int16(null))), avg(v2), count(v2), count(*))"    >> test.out

time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, uint8(null))), avg(v2), count(v2), count(*))"              >> test.out   
time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, int8(null))), avg(v2), count(v2), count(*))"               >> test.out
time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, int16(null))), avg(v2), count(v2), count(*))"              >> test.out
time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, uint16(null))), avg(v2), count(v2), count(*))"             >> test.out
time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, int32(null))), avg(v2), count(v2), count(*))"              >> test.out
time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, uint32(null))), avg(v2), count(v2), count(*))"             >> test.out
time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, int64(null))), avg(v2), count(v2), count(*))"              >> test.out
time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, uint64(null))), avg(v2), count(v2), count(*))"             >> test.out
time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, float(null))), avg(v2), count(v2), count(*))"              >> test.out
time iquery -aq "aggregate(apply(big_n_wild, v2, dcast(val, double(null))), avg(v2), count(v2), count(*))"             >> test.out

time iquery -aq "aggregate(apply(filter(apply(big_n_wild, v2, dcast(val, bool(null))), v2 is not null), v3, iif(v2, 1, 0)), sum(v3), count(*))" >> test.out


rm -rf /tmp/load_tools_test
mkdir /tmp/load_tools_test
touch /tmp/load_tools_test/empty_file
touch /tmp/load_tools_test/file1
echo 'col1    col2    col3
"alex"	1	3.5
"b"ob"	2	4.8
jake	4.0
random	3.1	"extra stuff"
bill 	abc	9
alice	4	not_a_number' >> /tmp/load_tools_test/file1
touch /tmp/load_tools_test/file2
echo '"",1
"abc",2
"def",3
null,4
xyz,4.5' >> /tmp/load_tools_test/file2
ln -s /tmp/load_tools_test/file1 /tmp/load_tools_test/symlink1
mkfifo /tmp/load_tools_test/fifo1
mkdir /tmp/load_tools_test/directory

#This is put here to help Jenkins configurations
chmod -R a+rwx /tmp/load_tools_test

iquery -aq "split('/tmp/load_tools_test/file1', 'lines_per_chunk=3')" >> test.out
iquery -aq "split('/tmp/load_tools_test/file1', 'lines_per_chunk=1')" >> test.out
iquery -aq "split('/tmp/load_tools_test/file1')"                      >> test.out
iquery -aq "split(
            'paths=/tmp/load_tools_test/file1;/tmp/load_tools_test/symlink1',
            'instances=1;2',
            'header=1',
            'lines_per_chunk=2'
            )" >> test.out
cat /tmp/load_tools_test/file2 > /tmp/load_tools_test/fifo1 &
iquery -aq "split(
            'paths=/tmp/load_tools_test/file1;/tmp/load_tools_test/symlink1;/tmp/load_tools_test/fifo1',
            'instances=1;2;0',
            'header=1',
            'lines_per_chunk=2'
            )" >> test.out
iquery -aq "split(
            'paths=/tmp/load_tools_test/file1;/tmp/load_tools_test/file2;/tmp/load_tools_test/directory',
            'instances=1;2;0',
            'lines_per_chunk=2', 
            'delimiter=,'
            )" >> test.out

iquery -anq "remove(foo)" > /dev/null 2>&1
iquery -anq "remove(bar)" > /dev/null 2>&1

iquery -anq "store(build(<val:double> [x=1:8000000,1000000,0], random()), foo)" > /dev/null
iquery -anq "save(foo, 'foo.tsv', -1, 'tsv')" > /dev/null
time iquery -anq "store(project(filter(apply(parse(split('paths=foo.tsv', 'instances=-1'), 'num_attributes=1'), v, dcast(a0, double(null))), v is not null), v), bar)" > /dev/null 
iquery -aq "op_count(bar)" >> test.out

iquery -aq "aio_input('/tmp/load_tools_test/file1', 'num_attributes=3')" >> test.out
iquery -aq "sort(aio_input('/tmp/load_tools_test/file1', 'num_attributes=3', 'buffer_size=40'), a0)" >> test.out
iquery -aq "sort(aio_input(
            'paths=/tmp/load_tools_test/file1;/tmp/load_tools_test/symlink1',
            'instances=1;2',
            'header=1',
            'num_attributes=2',
            'buffer_size=31'
            ),a0)" >> test.out
cat /tmp/load_tools_test/file2 > /tmp/load_tools_test/fifo1 &
iquery -aq "aio_input(
            'paths=/tmp/load_tools_test/file1;/tmp/load_tools_test/symlink1;/tmp/load_tools_test/fifo1',
            'instances=1;2;0',
            'header=1',
            'num_attributes=1'
            )" >> test.out
iquery -aq "sort(aio_input(
            'paths=/tmp/load_tools_test/file1;/tmp/load_tools_test/file2;/tmp/load_tools_test/directory',
            'instances=1;2;0',
            'buffer_size=41', 
            'attribute_delimiter=,',
            'num_attributes=3'
            ), a0)" >> test.out

iquery -anq "remove(bar)" > /dev/null 2>&1
time iquery -anq "store(project(filter(apply(aio_input('paths=foo.tsv', 'instances=-1', 'num_attributes=1'), v, dcast(a0, double(null))), v is not null), v), bar)" > /dev/null
iquery -aq "op_count(bar)" >> test.out

iquery -naq "remove(foo)" > /dev/null 2>&1
iquery -naq "store(apply(build(<v1:float null>[i=1:50,10,0], iif(i%2=0,null,i)), v2, double(i/10.4), v3, 'abcdef'), foo)" > /dev/null
iquery -anq "aio_save(sort(foo,v1,v2,v3,25), 'paths=/tmp/load_tools_test/foo;/tmp/load_tools_test/foo2', 'instances=2;0', 'format=tdv', 'cells_per_chunk=3')" >> test.out
iquery -aq "sort(input(foo, '/tmp/load_tools_test/foo',  0, 'tsv'), v2)" >> test.out
iquery -aq "sort(input(foo, '/tmp/load_tools_test/foo2', 0, 'tsv'), v2)" >> test.out
iquery -anq "aio_save(foo, '/tmp/load_tools_test/foo', 'format=(float null, double, string)', 'cells_per_chunk=8')" >> test.out
iquery -aq "sort(input(foo, '/tmp/load_tools_test/foo', 0, '(float null, double, string)'), v2)" >> test.out
iquery -anq "aio_save(between(sort(foo,v1,v2,v3), 0,9), 'paths=/tmp/load_tools_test/foo', 'instances=0', 'format=tdv', 'null_pattern=?%')" >> test.out
cat /tmp/load_tools_test/foo >> test.out
iquery -anq "aio_save(between(sort(foo,v1,v2,v3), 0,9), 'paths=/tmp/load_tools_test/foo', 'instances=0', 'format=tdv', 'null_pattern=NULL CODE %, BRUH')" >> test.out
cat /tmp/load_tools_test/foo >> test.out
iquery -anq "aio_save(between(sort(foo,v1,v2,v3), 0,9), 'paths=/tmp/load_tools_test/foo', 'instances=0', 'format=tdv', 'null_pattern=%, BRUH')" >> test.out
cat /tmp/load_tools_test/foo >> test.out
iquery -anq "aio_save(apply(show(foo), b, bool(true)),  'paths=/tmp/load_tools_test/foo', 'instances=0', 'format=tdv')" >> test.out
cat /tmp/load_tools_test/foo >> test.out
iquery -anq "aio_save(apply(show(foo), b, bool(true)),  'paths=/tmp/load_tools_test/foo', 'instances=1', 'format=tdv')" >> test.out
cat /tmp/load_tools_test/foo >> test.out
iquery -anq "aio_save(apply(show('filter(foo, \'abc\' = \'def\' )', 'afl'), b, bool(true)),  'path=/tmp/load_tools_test/foo', 'instances=1', 'format=csv+')" >> test.out
cat /tmp/load_tools_test/foo >> test.out

diff test.out test.expected
