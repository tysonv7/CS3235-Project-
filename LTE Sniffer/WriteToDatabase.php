<?php

		$dbhost = 'localhost:3306';
		$dbuser = 'root';
		$dbpass = 'ilovegeniussnorlax';
		//$serverName = "HUISHAN\SQLEXPRESS";
		$dbdatabase='LTESnifferDB';
		$tableName='LTESnifferTable';


		$frequency=$argv[1];
		$mcc=$argv[2];
		$mnc=$argv[3];
		$carrier=$argv[4];
		$physicalcellid=$argv[5];
		$psspower=$argv[6];
		echo "frequency $frequency $mcc $mnc $carrier $physicalcellid $psspower";

		
		$frequencyExistQuery="SELECT * FROM  $tableName  WHERE frequency=$frequency";
		$insertQuery = "INSERT INTO $tableName VALUES ($frequency, $mcc, $mnc, '$carrier', $physicalcellid, $psspower, now())";
		$updateQuery = "UPDATE $tableName SET mcc=$mcc, mnc=$mnc, carrier='$carrier', physical_cell_id=$physicalcellid, pss_power=$psspower, last_seen_date=now() WHERE frequency=$frequency";
		$params = array($frequency, $mcc, $mnc, '$carrier', $physicalcellid, $psspower, GETDATE());
		
		// Since UID and PWD are not specified in the $connectionInfo array,
		// The connection will be attempted using Windows Authentication.
		//$connectionInfo = array( "Database"=>$databaseName);
		$conn = mysql_connect( $dbhost, $dbuser, $dbpass);

		if( $conn ) {
		     echo "Connection established.<br />";
		}else{
		     echo "Connection could not be established.<br />";
		     die( mysql_error());
		}
		mysql_select_db($dbdatabase);

		//Check if frequency already exist in the database
		$checkFrequencyExist=mysql_query($frequencyExistQuery,$conn);
		echo "$checkFrequencyExist checking";
		if(!mysql_num_rows($checkFrequencyExist)){
			$result = mysql_query($insertQuery);
			if($result==false){
				echo "ERROR. Fail to add into database";
			}
			else{
				echo "Frequency $frequency mHz is added to database";
			}
		}
		else{
			$result=mysql_query($updateQuery,$conn);
			if($result==false){
				echo "ERROR. Fail to update into database";
			}
			else{
				echo "Frequency $frequency mHz is updated in the database";
			}
		}
		

?>
