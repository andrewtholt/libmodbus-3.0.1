--
--
--

drop table if exists config;
drop table if exists data;
drop table if exists rtu_data;
drop table if exists tag;
drop table if exists history;

drop trigger if exists clean_up;

create table config (   idx INTEGER PRIMARY KEY,
    retention_period integer default 30, -- days
    -- read data every . . .
    freq integer default 60, -- seconds

--    modbus_type varchar default "RTU" check ( modbus_type = "RTU" or modbus_type = "TCP" ),
--    ip_address varchar default "127.0.0.1",
--    ip_port int default 1502,

    tty_port varchar default "/dev/ttyUSB0",
    baud_rate integer default 9600 check (baud_rate = 9600 or baud_rate = 19200 or baud_rate = 4800 or baud_rate = 38400) ,
    parity  varchar(1) default "N" check ( parity = "E" or parity = "O" or parity = "N"),
    stop_bits integer default 2 check ( stop_bits = 1 or stop_bits = 2),
    length integer default 8 check ( length = 7 or length = 8)
);

-- create table data ( idx INTEGER PRIMARY KEY,
--     modbus_type varchar default "RTU" check ( modbus_type = "RTU" or modbus_type = "TCP" ),
--     ip_address varchar default "127.0.0.1",
--     ip_port int default 1502,
-- 
--     RTU integer check(RTU > 0 and RTU < 255),
--     phaseA integer default 0,
--     phaseB integer default 0,
--     phaseC integer default 0,
--     
--     phaseAKWH integer default 0,
--     phaseBKWH integer default 0,
--     phaseCKWH integer default 0,
--     timeStamp DATE default CURRENT_TIMESTAMP 
-- );

create table tag ( idx INTEGER PRIMARY KEY,
    --
    -- Tag name
    --
    name varchar,
    -- 
    -- MQTT Topic.  May be empty
    --
    topic varchar,
    --
    -- Modbus function number
    --
    modbus_function integer default 4,
    --
    -- ModBus address
    --
    address integer check(address < 65535),
    -- 
    -- Number of registers to read
    --
    registers integer check(registers > 0 and registers < 100)
    -- data goes here
);


create table rtu_data ( idx INTEGER PRIMARY KEY,
    RTU integer check(RTU > 0 and RTU <= 255),
    tag varchar unique,
    enabled boolean default 1,
    log boolean default 1,
    -- Time stuff.
    --
    scan_time integer check (scan_time > 9 and scan_time < 3600),
    offset integer default 0 check ( offset >= 0 and offset < 3600),
    ttl integer default 0,
    readings integer,
    
    timeStamp DATE default CURRENT_TIMESTAMP     
);

create table history ( idx INTEGER primary KEY,
    RTU integer check(RTU > 0 and RTU <=255 ),
    tag varchar,
    readings integer,
    
    timestamp DATE default CURRENT_TIMESTAMP
);

--create trigger log_it after update on rtu_data
--begin
--    insert into history (RTU,tag,readings,timestamp) 
--    select RTU,tag,readings,timestamp from rtu_data where ttl=0 and enabled=1 and log=1;
--end;

--create trigger clean_up after insert on history
--begin
----    update data set timeStamp = DATETIME('NOW') WHERE rowid = new.rowid;
--    delete from data where idx <= 
--    ( (select max(idx) from data) - (select ((86400 / freq) * retention_period ) from config)
--    );
--end;

--
-- Retain data for 30 days, the default for column freq 
-- is currently 60 seconds. i.e. by default the table will grow to
-- 60 * 24 * 30 = 43200 rows
--
insert into config ( retention_period ) values ( 30 );

-- insert into data (RTU) values ( 1 );
-- insert into data (RTU) values ( 2 );

insert into tag ( name, topic, address, registers) values ("PorchLight","/home/Outside/PorchLight/Power", 0,1);
insert into tag ( name, topic, address, registers) values ("BackFloodlight","/home/outside/BackFloodlight/cmnd/power",1,1);
insert into tag ( name,address,registers) values ("CurrentC",2,1);

insert into rtu_data (RTU,tag,scan_time) values (1,"PorchLight",10);
insert into rtu_data (RTU,tag,scan_time) values (1,"BackFloodlight",10);
insert into rtu_data (RTU,tag,scan_time) values (1,"CurrentC",10);


