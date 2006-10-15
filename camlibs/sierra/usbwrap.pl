#!/usr/bin/perl
# Copyright Marcus Meissner 2005.
# Licensed under GPL v2. NO WARRANTY.
# 
# This is mostly unfinished ...

use strict;
use XML::Parser;
use IO::Handle;

my $xmlfile = shift @ARGV || die "specify xml file on cmdline:$!\n";

my @elemstack = ();
my @data = 0;
my $needdata = 0;
my $curseq = -1;
my $curep = -1;
my %urbenc = ();
my @lastresp = ();
my @lastreq = ();
my $lastcode;
my $vendorid = 0;

my $size=-s $xmlfile;
my $buffer;
sysopen(BINFILE,$xmlfile,0) || die "sysopen: $!";
sysread(BINFILE,$buffer,$size);
close(BINFILE);

my @data = unpack("C*",$buffer);
if ($data[0] == 60) { # 60 == '<' ... start of XML file ...
	my $p1 = new XML::Parser(
		 Handlers => {	Start => \&xml_handle_start,
				End   => \&xml_handle_end,
				Char  => \&xml_handle_char}
	);
	$p1->parsefile($xmlfile);
	exit 0;
}
print "No XML.\n";
exit 1;

sub hexdump {
	my @data = @_;
	my $i;
	my $str = "";
	for ($i = 0;$i <= $#data ; $i++) {
		my $c = $data[$i];
		if (($i & 0x0f) == 0) {
			if ($i) {
				print " $str\n";
				$str = "";
			}
			printf "%03x: ", $i;
		}
		printf " %02x", $c;
		if (($c >= 0x20) && ($c < 0x7f)) {
			$str .= sprintf "%c", $c;
		} else {
			$str .= ".";
		}
	}
	printf " $str\n";
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
	return $str;
}

my $lastfunction;
my $lasttype = 0;

sub xml_handle_char {
	my ($expat, $str) = @_;
	my @bytes;

	if ($elemstack[$#elemstack] eq "function") {
		$lastfunction = $str;
		return;
	}
	if ($elemstack[$#elemstack] eq "endpoint") {
		$curep = $str;
		return;
	}
	return unless ($elemstack[$#elemstack] eq "payloadbytes");
	if ($lastfunction ne "BULK_OR_INTERRUPT_TRANSFER") {
		print "str $str\n";
		return;
	}
	# for the IN ep we only get the second mention of the URB
	# for the OUT ep we only get the first.
	if ($curep & 0x80) {
		return if ($urbenc{$curseq} == 1);
	} else {
		return if ($urbenc{$curseq} == 2);
	}
	@bytes = unpack('C*',pack('H*',"\U$str"));
	# print "xxx\n"; hexdump( @bytes ); print "yyy\n";
	my $a = shift @bytes; my $b = shift @bytes;
	my $c = shift @bytes; my $d = shift @bytes;
	if (($a == 0x55) && ($b == 0x53) && ($c == 0x42) && ($d == 0x43)) {
		print_wrap_request(\@lastreq);
		@lastreq = @bytes;
		return;
	}
	if (($a == 0x55) && ($b == 0x53) && ($c == 0x42) && ($d == 0x53)) {
		print_wrap_response(\@lastresp,\@bytes);
		@lastresp = ();
		return;
	}
	if ($curep & 0x80) {
		push @lastresp, $a, $b, $c, $d, @bytes;
	} else {
		push @lastreq, $a, $b, $c, $d, @bytes;
	}
}

sub xml_handle_start {
	my ($expat, $element, $attr, $val) = @_;
	if (($element eq "urb") && ($attr eq "sequence")) {
		$curseq = $val;
		$urbenc{$val} = $urbenc{$val} + 1;
	}
	push @elemstack, $element;
}

sub xml_handle_end {
	my ($expat, $element) = @_;
	pop @elemstack;
}

sub print_wrap_request {
	my ($bytesref) = @_;
	my @xbytes = @{$bytesref};
	my ($length, $cursq, $inout, $a, $len, $cmd);

	$cursq = get_uint32(\@xbytes);
	$length = get_uint32(\@xbytes);

	return if (!$#xbytes);

	$inout = get_uint8(\@xbytes);
	$a = get_uint8(\@xbytes);
	$len = get_uint8(\@xbytes);
	$cmd = get_uint8(\@xbytes);
	if (($cmd & 0xf0) != 0xc0) {
		# printf "request: cmd is %x?\n", $cmd;
		return;
	}
	get_uint32(\@xbytes); 	# zero
	get_uint32(\@xbytes); 	# zero2
	my $xlength = get_uint32(\@xbytes); 
	get_uint8(\@xbytes); get_uint8(\@xbytes); get_uint8(\@xbytes); 
	if (($cmd & 0x0f) == 0) {
		# print ">RDY: "; hexdump(@xbytes);
	} elsif (($cmd & 0x0f) == 1) {
		print ">CMND: ";
		splice @xbytes, 0, 64;
		# now this only has the Sierra Command.
		hexdump(@xbytes);
	} elsif (($cmd & 0x0f) == 2) {
		if ($#xbytes>-1) {
			print ">DATA: "; hexdump(@xbytes);
		}
	} elsif (($cmd & 0x0f) == 3) {
		# print ">STAT: "; hexdump(@xbytes);
	} elsif (($cmd & 0x0f) == 4) {
		splice @xbytes, 0, 12;
		my $size = get_uint32(\@xbytes);
		# print ">SIZE: $size\n";
	} else {
		# printf ">Unknown(%x): ", $cmd; hexdump(@xbytes);
	}
}

sub print_wrap_response {
	my ($dataref,$bytesref) = @_;
	my @xdata = @{$dataref};
	my @xbytes = @{$bytesref};
	my ($curseq, $shouldbe0);

	$curseq = get_uint32(\@xbytes);
	$shouldbe0 = get_uint32(\@xbytes);
	if ($shouldbe0 != 0) {
		print "RESPONSE, but response should be 0?\n";
		return;
	}
	return if ($#xdata==-1);
	# print "response data: ";hexdump(@xdata);
	my @blub = @xdata;
	my $length = get_uint32(\@xdata);
	my $c0 = get_uint8(\@xdata);
	my $c1 = get_uint8(\@xdata);
	my $c2 = get_uint8(\@xdata);
	my $c3 = get_uint8(\@xdata);
	if (!(($c2 == 0xff) && ($c3 == 0x9f))) {
		#print "response unknown?, $c2, $c3\n"; hexdump(@blub);
		return;
	}
	if ($c0 == 0x01) {
		# print "<RDY:"; hexdump (@xdata);
		return;
	} elsif ($c0 == 0x02) {
		splice @xdata,0,64-8;
		if ($#xdata>-1) {
			print "<DATA:";
			hexdump (@xdata);
		}
	} elsif ($c0 == 0x03) {
		#print "<STAT:"; hexdump (@xdata);
		return;
	} else {
		printf "<Unknown(%x):", $c0;
		hexdump (@xdata);
	}
}
