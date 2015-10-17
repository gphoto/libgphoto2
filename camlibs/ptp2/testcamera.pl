#!/usr/bin/perl -w
use strict;

my $gphoto2 = "gphoto2";

my %camera = ();


$camera{'havecapture'} 		= 1;
$camera{'havecapturetarget'} 	= 1;
my @formats = ();

$camera{'imageformats'} = \@formats;

# returns TRUE on success, FALSE on fail
sub run_gphoto2(@) {
	my @cmdline = @_;

	print STDERR "running: " . join(" ",$gphoto2,@cmdline) . "\n";

	print LOGFILE "running: " . join(" ",$gphoto2,@cmdline) . "\n";
	open(GPHOTO,'-|',$gphoto2,@cmdline)||die "open $gphoto2";
	while (<GPHOTO>) {
		print;
		print LOGFILE;
	}
	my $rc = close(GPHOTO);
	print LOGFILE "returned: $rc\n";
	return $rc;
}

sub ok($$) {
	my ($testres,$text) = @_;

	if ($testres) {
		print "$text: SUCCESS\n";
	} else {
		print "$text: FAIL\n";
	}
}

my $workdir = `mktemp -d /tmp/testcamera.XXXXXX`;
chomp $workdir;
die "no workdir created" unless -d $workdir;
chdir ($workdir);

print "Using temporary directory: $workdir\n";

open(LOGFILE,">logfile.testcase")||die;

if (!run_gphoto2("-L")) {
	print LOGFILE "No camera attached?\n";
	close(LOGFILE);
	die "-L does not work, reason see above\n";
}

ok(run_gphoto2("-L"),"testing -L");
ok(run_gphoto2("-l"),"testing -l");
ok(run_gphoto2("--summary"),"testing --summary");

sub run_gphoto2_capture(@) {
	my $text = pop;
	my @cmd = @_;

	if ($camera{'havecapturetarget'}) {
		my @newcmd = @cmd;
		unshift @newcmd,"--set-config-index","capturetarget=0";
		ok(run_gphoto2(@newcmd),"$text: " . join(" ",@newcmd));
		@newcmd = @cmd;
		unshift @newcmd,"--set-config-index","capturetarget=1";
		ok(run_gphoto2(@newcmd),"$text: " . join(" ",@newcmd));
	} else {
		ok(run_gphoto2("--capture-image-and-download"),$text);
	}
}

run_gphoto2_capture("--capture-image-and-download","simple capture and download");
