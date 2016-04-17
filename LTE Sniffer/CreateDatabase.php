<?php

	$hostname='localhost:3306';
	$username='root';
	$mysql_password='ilovegeniussnorlax';
	$dbname="LTESnifferDB";
	$tablename="LTESnifferTable";

	$link = mysql_connect($hostname,$username,$mysql_password);
	if(!$link) {
		die('Could not connect: ' . mysql_error());
	}else{
		echo "Connected";
	}

	$sql = "CREATE DATABASE $dbname";
	if(mysql_query($sql,$link)){
		echo "Database $dbname created successfully\n";
	}else{
		echo 'Error creating database: ' . mysql_error() . "\n";
	}
	mysql_select_db($dbname);

	$tablesql = "CREATE TABLE $tablename(frequency DECIMAL(10,2) NOT NULL PRIMARY KEY, mcc INT, mnc INT, carrier VARCHAR(20), physical_cell_id INT, pss_power DECIMAL(10,2), last_seen_date date)";

	if(mysql_query($tablesql,$link)){
		echo "Created";
	} else{
		echo "ERROR: Could not create. " . mysql_error();
	}

	mysql_close($link); 


?>
