#!/usr/bin/perl -w
use strict;

$ENV{'LANG'} = 'C';

my $gphoto2 = "gphoto2";

my %eos100d = ();

my %formats = (
	"jpg" => "imageformat=0",
	"raw" => "imageformat=9",
	"both" => "imageformat=8",
);
$eos100d{'imageformats'} = \%formats;

my %camera = %eos100d;

##################################################

# auto configured
my $havecapture = 0;
my $havetriggercapture = 0;
my $havepreview = 0;
my $havecapturetarget = 0;

my @lastresult = ();
# returns TRUE on success, FALSE on fail
sub run_gphoto2(@) {
	my @cmdline = @_;

	@lastresult = ();
	print STDERR "running: " . join(" ",$gphoto2,@cmdline) . "\n";

	print LOGFILE "running: " . join(" ",$gphoto2,@cmdline) . "\n";
	open(GPHOTO,'-|',$gphoto2,@cmdline)||die "open $gphoto2";
	while (<GPHOTO>) {
		push @lastresult, $_;
		print;
		print LOGFILE;
		chomp;
		push @lastresult, $_;
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

sub run_gphoto2_capture_target($$@) {
	my ($nrimages,$text,@cmd) = @_;

	ok(&run_gphoto2(@cmd),"$text: " . join(" ",@cmd));

	my @files = <*>;
	if ($nrimages+1 != @files) {
		print STDERR "*** expected $nrimages files, got $@files\n";
	}
	&remove_all_files();
}

sub run_gphoto2_capture($$@) {
	my ($nrimages,$text,@cmd) = @_;
	my @newcmd = @cmd;

	print "run_gphoto2_capture: havecapturetarget $havecapturetarget\n";
	if ($havecapturetarget) {
		my @newcmd = @cmd;
		unshift @newcmd,"--set-config-index","capturetarget=0";
		&run_gphoto2_capture_target($nrimages,$text,@newcmd);

		@newcmd = @cmd;
		unshift @newcmd,"--set-config-index","capturetarget=1";
		&run_gphoto2_capture_target($nrimages,$text,@newcmd);
	} else {
		&run_gphoto2_capture_target($nrimages,$text,@newcmd);
	}
}

sub run_gphoto2_capture_formats($$@) {
	my ($nrimages,$text,@cmd) = @_;

	if ($camera{'imageformats'}) {
		my @newcmd = @cmd;
		my %formats = %{$camera{'imageformats'}};

		foreach my $format (sort keys %formats) {
			print "testing image $format\n";
			@newcmd = @cmd;
			unshift @newcmd,"--set-config-index",$formats{$format};

			if ($format eq "jpg") {
				&run_gphoto2_capture($nrimages,$text,@newcmd);
			} elsif ($format eq "both") {
				&run_gphoto2_capture($nrimages*2,$text,@newcmd);
				unshift @newcmd,"--keep-raw";
				&run_gphoto2_capture($nrimages,$text,@newcmd);
			} elsif ($format eq "raw") {
				&run_gphoto2_capture($nrimages,$text,@newcmd);
				unshift @newcmd,"--keep-raw";
				&run_gphoto2_capture(0,$text,@newcmd);
			}
		}
		&run_gphoto2("--set-config-index",$formats{'jpg'});
	} else {
		&run_gphoto2_capture($nrimages,$text,@cmd);
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

# Basics
ok(run_gphoto2("-L"),"testing -L");
ok(run_gphoto2("-l"),"testing -l");

# Autodetect capabilities: capture, preview or trigger capture
ok(run_gphoto2("-a"),"testing -a");
my @abilities = @lastresult;

$havecapture = 1 if (grep (/Capture/,@abilities));
$havetriggercapture = 1 if (grep (/Trigger Capture/,@abilities));
$havepreview = 1 if (grep (/Preview/,@abilities));

ok(run_gphoto2("--summary"),"testing --summary");
ok(run_gphoto2("--list-all-config"),"testing --list-all-config");

# Autodetect if some config variables are present

ok(run_gphoto2("--list-config"),"testing --list-config");
my @allconfig = @lastresult;

$havecapturetarget = 1 if (grep(/capturetarget/,@allconfig));

my $havedatetime = 1 if (grep(/datetime/,@allconfig));
if ($havedatetime) {
	ok(run_gphoto2("--get-config","datetime"),"testing --get-config datetime before setting");
	ok(run_gphoto2("--set-config","datetime=now"),"testing --set-config datetime=now");
	ok(run_gphoto2("--get-config","datetime"),"testing --get-config datetime after setting");
}

# Capture stuff lots of it

if ($havecapture) {
	run_gphoto2_capture_formats(1,"simple capture and download","--capture-image-and-download");
	run_gphoto2_capture_formats(3,"multiframe capture and download","--capture-image-and-download","-F 3","-I 2");
	run_gphoto2_capture_formats(1,"waitevent","--wait-event-and-download=10s");
}
if ($havetriggercapture) {
	run_gphoto2_capture_formats(1,"trigger capture","--trigger-capture","--wait-event-and-download=5s");
}
if ($havepreview) {
	ok(run_gphoto2("--capture-preview"),"testing --capture-preview");
	remove_all_files();
}
