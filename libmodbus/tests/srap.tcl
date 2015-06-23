package require sqlite3

set dbName "/var/data/RS.db"

set rowLength 4
sqlite3 db1 $dbName

set sql "select idx,phaseA,phaseB,phaseC from data;"

set data [ db1 eval { select idx,phaseA,phaseB,phaseC from data where rtu=1 } ]

for {set i 0} { $i < [ llength $data ] } { set i [ expr $i +4 ] } {
    set res ""
    append res [ lindex $data $i ]
    append res " "
    append res [ lindex $data [ expr $i + 1 ] ]
    append res " "
    append res [ lindex $data [ expr $i + 2 ] ]
    append res " "
    append res [ lindex $data [ expr $i + 3 ] ]
    puts $res
}
puts ""

puts [ llength $data ]
