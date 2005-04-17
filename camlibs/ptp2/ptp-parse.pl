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

sub handle_start {
	my ($expat, $element, $attr, $val) = @_;
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

	print "len $len\n";
	$str = '';
	for ($i = 0; $i < $len ; $i++ ) {
		$str .= pack("C",shift(@{$arrref}));
		shift @{$arrref};
	}
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
	return unless ($elemstack[$#elemstack] eq "payloadbytes");
	if ($lastfunction ne "BULK_OR_INTERRUPT_TRANSFER") {
		print "str $str\n";
		return;
	}

	@bytes = unpack('C*',pack('H*',"\U$str"));

	my $length = get_uint32(\@bytes);
	my $type = get_uint16(\@bytes);
	if ($type == 1) {
		print "COMMAND: ";
	} elsif ($type == 2) {
		print "str is $str\n";
		print "DATA: ";
	} elsif ($type == 3) {
		print "RESPONSE: ";
	} elsif ($type == 4) {
		print "EVENT: ";
	} else {
		# printf "TYPE %02x: ", $type;
		print "str: $str\n";
		return;
	}
	my $code	= get_uint16(\@bytes);
	my $transid	= get_uint32(\@bytes);
	# not really needed .... print "transid $transid ";

	if ($code == 0x1001) {
		print "GetDeviceInfo(1001) ... \n";
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
		if ($type ==1 ) {
			print "GetStorageIds(1004)\n";
		} elsif ($type == 2) {
			my $nr;
			my $i;

			$nr = get_uint32(\@bytes);
			print "GetStorageIds(1004) ";
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
			print "GetStorageInfo(1005)\n";
		} elsif ($type == 2) {
			my $storagetype		= get_uint16(\@bytes);
			my $filesystemtype	= get_uint16(\@bytes);
			my $accesscap		= get_uint16(\@bytes);
			my $maxcap		= get_uint32(\@bytes);	$dummy		= get_uint32(\@bytes);
			my $freespace		= get_uint32(\@bytes);	$dummy		= get_uint32(\@bytes);
			my $freeimages		= get_uint32(\@bytes);

			print "GetStorageInfo(1005) type $storagetype, filesystem $filesystemtype, accesscap $accesscap, freespace $freespace, freeimages $freeimages\n";
		}
		return;
	} elsif ($code == 0x1014) {
		if ($type == 1) {
			my $prop = get_uint16(\@bytes);
			printf "GetDevicePropDesc(1014) prop = %04x\n", $prop;
		} elsif ($type == 2) {
			print "GetDevicePropDesc(1014)\n";
		}
		return;
	} elsif ($code == 0x1015) {
		if ($type == 1) {
			my $prop = get_uint32(\@bytes);
			printf "GetDevicePropValue(1015) prop = %04x\n", $prop;
		} elsif ($type == 2) {
			print "GetDevicePropValue(1015) data ". join(",",@bytes) . "\n";
		}
		return;
	} elsif ($code == 0x1016) {
		if ($type == 1) {
			my $prop = get_uint32(\@bytes);
			printf "SetDevicePropValue(1016) prop = %04x, " . join(",",@bytes) . "\n", $prop;
		} elsif ($type == 2) {
			print "SetDevicePropValue(1016) data ". join(",",@bytes) . "\n";
		}
		return;
	} elsif ($code == 0x1007) {
		if ($type ==1 ) {
			print "GetObjectHandles(1007)\n";
		} elsif ($type == 2) {
			my $sid		= get_uint32(\@bytes);
			my $ofc		= get_uint32(\@bytes);
			my $assoc	= get_uint32(\@bytes);
			printf("GetObjectHandles(1007) 0x%08x 0x%08x 0x%08x\n", $sid, $ofc, $assoc);
		}
		return;
	} elsif ($code == 0x1008) {
		if ($type ==1 ) {
			print "GetObjectInfo(1008)\n";
		} elsif ($type == 2) {
			printf("GetObjectInfo(1008) ...\n");
		}
		return;
	} elsif ($code == 0x9013) {
		if ($type == 1) {
			print "CANON CheckEvent(9013)\n";
		} elsif ($type == 2) {
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
