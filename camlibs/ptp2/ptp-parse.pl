#!/usr/bin/perl
# Copyright Marcus Meissner 2005.
# Licensed under GPL v2. NO WARRANTY.
# 
# This is unfinished ... It does not correctly read the data.

use strict;
use XML::Parser;

my $xmlfile = shift @ARGV || die "specify xml file on cmdline:$!\n";
my $p1 = new XML::Parser(
         Handlers => {	Start => \&handle_start,
			End   => \&handle_end,
			Char  => \&handle_char}
);
$p1->parsefile($xmlfile);


my @elemstack = ();
my @data = 0;
my $needdata = 0;
my $curseq = -1;
my $curep = -1;
my %urbenc = ();
my @curdata = ();
my $lastcode = 0;

sub handle_start {
	my ($expat, $element, $attr, $val) = @_;
	if (($element eq "urb") && ($attr eq "sequence")) {
		$curseq = $val;
		$urbenc{$val} = $urbenc{$val} + 1;
	}
	push @elemstack, $element;
}

sub handle_end {
	my ($expat, $element) = @_;
	pop @elemstack;
}

sub get_uint32 {
	my($arrref) = @_;

	return	shift(@{$arrref})+256*(shift(@{$arrref}))+
		256*256*shift(@{$arrref})+256*256*256*(shift(@{$arrref}));
}

sub get_uint16 {
	my($arrref) = @_;

	return shift(@{$arrref})+256*(shift(@{$arrref}));
}

sub get_uint8 {
	my($arrref) = @_;

	return shift @{$arrref};
}

sub get_str {
	my($arrref) = @_;
	my $len = get_uint8($arrref);
	my $i;
	my $str;

	$str = '';
	for ($i = 0; $i < $len - 1 ; $i++ ) {
		$str .= pack("C",shift(@{$arrref}));
		shift @{$arrref};
	}
	# remove terminating 0x00 0x00
	shift @{$arrref};
	shift @{$arrref};
	return $str;
}

my $lastfunction;

sub handle_char {
	my ($expat, $str) = @_;
	my $dummy;
	my @bytes;

	if ($elemstack[$#elemstack] eq "function") {
		$lastfunction = $str;
		return;
	}
	if ($elemstack[$#elemstack] eq "endpoint") {
		$curep = $str;
		return;
	}
	# for the IN ep we only get the second mention of the URB
	# for the OUT ep we only get the first.
	if ($curep == 129) {
		return if ($urbenc{$curseq} == 1);
	} else {
		if ($curep == 2) {
			return if ($urbenc{$curseq} == 2);
		}
	}
	return unless ($elemstack[$#elemstack] eq "payloadbytes");
	if ($lastfunction ne "BULK_OR_INTERRUPT_TRANSFER") {
		print "str $str\n";
		return;
	}

	@bytes = unpack('C*',pack('H*',"\U$str"));

	my $length = get_uint32(\@bytes);
	my $type = get_uint16(\@bytes);

	if ($type == 1) {
		print "$curseq: COMMAND: ";
	} elsif ($type == 2) {
		# print "str is $str\n";
		# print "DATA: ";
	} elsif ($type == 3) {
		print "$curseq: RESPONSE: ";
	} elsif ($type == 4) {
		print "$curseq: EVENT: ";
	} else {
		# printf "$curseq: TYPE %02x: ", $type;
		# print "$str\n";
		@bytes = unpack('C*',pack('H*',"\U$str"));
		push @curdata, @bytes;
		return;
	}
	my $code	= get_uint16(\@bytes);
	my $transid	= get_uint32(\@bytes);
	# not really needed .... print "transid $transid ";

	if ($type == 1) {
		$lastcode = $code;
		@curdata = ();
	}
	if ($type == 2) {
		push @curdata, @bytes;
		return;
	}

	if ($type == 3) {
		if ($code == 0x2001) {
			print "OK(2001) ";
		}
		$code = $lastcode;
	}


	# evaluate commands
	# type == 1 	during send ... print parameters sent to camera
	# type == 3	after response ... print data returned
	#		@bytes - bulk container in response (no data)
	#		@curdata - bulk data if command had seperate datastream

	if ($code == 0x1001) {
		if ($type == 1) {
			print "GetDeviceInfo(1001)\n";
		} else {
			print "GetDeviceInfo(1001) data=" . join(",",@curdata) . "\n";
		}
		return;
	} elsif ($code == 0x9008) {
		print "CANON Start Shooting Mode (9008)\n";
		return;
	} elsif ($code == 0x900b) {
		print "CANON ViewFinder On(900b)\n";
		return;
	} elsif ($code == 0x900b) {
		print "CANON ViewFinder Off(900c)\n";
		return;
	} elsif ($code == 0x1002) {
		printf "OpenSession(1002) id = 0x%08x\n", get_uint32(\@bytes);
		return;
	} elsif ($code == 0x1004) {
		if ($type == 1) {
			printf "GetStorageIds(1004)\n";
		} elsif ($type == 3) {
			my $nr;
			my $i;

			$nr = get_uint32(\@bytes);
			printf "GetStorageIds(1004) [%x]=", $nr;
			for ($i = 0; $i < $nr; $i++) {
				printf("0x%08x",get_uint32(\@bytes));
				if ($i != $nr-1) {
					print ",";
				}
			}
			print "\n";
		}
		return;
	} elsif ($code == 0x1005) {
		if ($type ==1 ) {
			my $sid = get_uint32(\@bytes);
			printf "GetStorageInfo(1005) storage=%08lx\n,$sid";
		} elsif ($type == 3) {
			my $storagetype		= get_uint16(\@curdata);
			my $filesystemtype	= get_uint16(\@curdata);
			my $accesscap		= get_uint16(\@curdata);
			my $maxcap		= get_uint32(\@curdata);	$dummy		= get_uint32(\@curdata);
			my $freespace		= get_uint32(\@curdata);	$dummy		= get_uint32(\@curdata);
			my $freeimages		= get_uint32(\@curdata);

			print "GetStorageInfo(1005) type $storagetype, filesystem $filesystemtype, accesscap $accesscap, freespace $freespace, freeimages $freeimages\n";
		}
		return;
	} elsif ($code == 0x1014) {
		if ($type == 1) {
			my $prop = get_uint16(\@bytes);
			printf "GetDevicePropDesc(1014) prop = %04x\n", $prop;
		} elsif ($type == 3) {
			print "GetDevicePropDesc(1014)\n";
		}
		return;
	} elsif ($code == 0x1015) {
		if ($type == 1) {
			my $prop = get_uint32(\@bytes);
			printf "GetDevicePropValue(1015) prop = %04x\n", $prop;
		} elsif ($type == 3) {
			print "GetDevicePropValue(1015) data ". join(",",@bytes) . "\n";
		}
		return;
	} elsif ($code == 0x1016) {
		if ($type == 1) {
			my $prop = get_uint32(\@bytes);
			printf "SetDevicePropValue(1016) prop = %04x, " . join(",",@bytes) . "\n", $prop;
		} elsif ($type == 3) {
			print "SetDevicePropValue(1016) data ". join(",",@bytes) . "\n";
		}
		return;
	} elsif ($code == 0x1007) {
		if ($type == 1) {
			my $sid		= get_uint32(\@bytes);
			my $ofc		= get_uint32(\@bytes);
			my $assoc	= get_uint32(\@bytes);
			printf("GetObjectHandles(1007) 0x%08x 0x%08x 0x%08x\n", $sid, $ofc, $assoc);
		} elsif ($type == 3) {
			print "GetObjectHandles(1007) . "+join(",",@curdata) . "\n";
		}
		return;
	} elsif ($code == 0x1008) {
		if ($type ==1 ) {
			my $obid	= get_uint32(\@bytes);
			my $xx		= get_uint32(\@bytes);
			printf("GetObjectInfo(1008) object=0x%08lx, xx=0x%08lx\n", $obid, $xx);
		} elsif ($type == 3) {
			my $sid		= get_uint32(\@curdata);	# 0
			my $of		= get_uint16(\@curdata);	# 4
			my $protect	= get_uint16(\@curdata);	# 6
			my $compsize	= get_uint32(\@curdata);	# 8
			my $thumbof	= get_uint16(\@curdata);	# 12
			my $thumbsize	= get_uint32(\@curdata);	# 14
			my $thumbwidth	= get_uint32(\@curdata);	# 18
			my $thumbheight	= get_uint32(\@curdata);	# 22
			my $width	= get_uint32(\@curdata);	# 26
			my $height	= get_uint32(\@curdata);	# 30
			my $depth	= get_uint32(\@curdata);	# 34
			my $parent	= get_uint32(\@curdata);	# 38
			my $assoctype	= get_uint16(\@curdata);	# 42
			my $assocdesc	= get_uint32(\@curdata);	# 44
			my $seqnr	= get_uint32(\@curdata);	# 48
			my $filename	= get_str(\@curdata);
			my $capdate	= get_str(\@curdata);
			my $moddate	= get_str(\@curdata);

			print "GetObjectInfo(1008)\n";
			print "\tFileName: $filename\n";
			print "\tcapturedate: $capdate\n";
			print "\tmodificationdate: $moddate\n";
			printf "\tStorageID: %08lx\n", $sid;
			printf "\tOFC: %04x\n",$of;
		}
		return;
	} elsif ($code == 0x9014) {
		if ($type ==1 ) {
			print "FocusLock(9014)\n";
		} elsif ($type == 3) {
			printf("FocusLock(9014) ...\n");
		}
		return;
	} elsif ($code == 0x9015) {
		if ($type ==1 ) {
			print "FocusUnlock(9015)\n";
		} elsif ($type == 3) {
			printf("FocusUnlock(9015) ...\n");
		}
		return;
	} elsif ($code == 0x9020) {
		if ($type ==1 ) {
			print "CANON GetChanges(9020)\n";
		} elsif ($type == 3) {
			print "CANON GetChanges(9020) ";
			my $len = get_uint32(\@bytes);
			my $i;
			print " codes[$len] = {";
			for ($i = 0; $i < $len ; $i++) {
				my $code = get_uint16(\@bytes);
				printf("%04x,",$code);
			}
			print "}\n";
		}
		return;
	} elsif ($code == 0x9013) {
		if ($type == 1) {
			print "CANON CheckEvent(9013)\n";
		} elsif ($type == 3) {
			my $i;
			my $len = get_uint32(\@bytes);
			my $type = get_uint16(\@bytes);
			my $code = get_uint16(\@bytes);
			my $transid = get_uint32(\@bytes);
			printf("CANON CheckEvent(9013) len = $len, type = %04x, code = %04x, ", $type, $code);
			my $x;

			if ($len > 12 + 4*4) {
				$len = 12+4*4;
			}

			for  ($i = 12; $i < $len ; $i += 4 ) {
				$x = get_uint32(\@bytes);
				printf ("%08x,", $x);
			}
			print "\n";
		}
		return;
	} elsif ($code == 0x2001) {
		print "OK(2001) ";
			
		while ($#bytes > 0) {
			printf "0x%08x", get_uint32(\@bytes);
			print "," if ($#bytes >0);
		}
		print "\n";
		return;
	} else {
		printf "%04x ", $code;
	}
	print join(",",@bytes) . "\n";
	# print "str: $str\n";
}
