#!/usr/bin/perl -w
use strict;

my $gphoto2 = "gphoto2";

my %camera = ();


$camera{'havecapture'} 		= 1;
$camera{'havecapturetarget'} 	= 1;
$camera{'havecapturetarget'} 	= 1;
my %formats = {
	"jpg" => "imageformat=0",
	"raw" => "imageformat=9",
	"both" => "imageformat=8",
};

$camera{'imageformats'} = \%formats;

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

sub remove_all_files {
	my @files = <*>;

	foreach my $file (@files) {
		next if ($file =~ /logfile/);
		print STDERR "deleting $file\n";
		unlink $file;
	}
}

sub run_gphoto2_capture_target(@) {
	my $text = pop;
	my $nrimages = pop;
	my @cmd = @_;

	ok(&run_gphoto2(@cmd),"$text: " . join(" ",@cmd));

	my @files = <*>;
	if ($nrimages+1 != @files) {
		print STDERR "*** expected $nrimages files, got $@files\n";
	}
	&remove_all_files();
}

sub run_gphoto2_capture(@) {
	my @cmd = @_;
	my @newcmd = @cmd;

	if ($camera{'havecapturetarget'}) {
		my @newcmd = @cmd;
		unshift @newcmd,"--set-config-index","capturetarget=0";
		&run_gphoto2_capture_target(@newcmd);

		@newcmd = @cmd;
		unshift @newcmd,"--set-config-index","capturetarget=1";
		&run_gphoto2_capture_target(@newcmd);
	} else {
		&run_gphoto2_capture_target(@newcmd);
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
ok(run_gphoto2("--list-all-config"),"testing --list-all-config");
ok(run_gphoto2("--list-config"),"testing --list-config");

if ($camera{'havecapture'}) {
	run_gphoto2_capture("--capture-image-and-download",1,"simple capture and download");
	run_gphoto2_capture("--capture-image-and-download","-F 5","-I 3",5,"simple capture and download");
}
