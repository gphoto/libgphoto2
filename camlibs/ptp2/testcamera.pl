#!/usr/bin/perl -w
use strict;

my $gphoto2 = "gphoto2";

my %camera = ();


$camera{'havecapture'} 		= 1;
$camera{'havecapturetarget'} 	= 1;
$camera{'havecapturetarget'} 	= 1;
my %formats = (
	"jpg" => "imageformat=0",
	"raw" => "imageformat=9",
	"both" => "imageformat=8",
);

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

	if ($camera{'havecapturetarget'}) {
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
				&run_gphoto2_capture_target($nrimages,$text,@newcmd);
			} elsif ($format eq "both") {
				&run_gphoto2_capture_target($nrimages*2,$text,@newcmd);
				unshift @newcmd,"--keep-raw";
				&run_gphoto2_capture_target($nrimages,$text,@newcmd);
			} elsif ($format eq "raw") {
				&run_gphoto2_capture_target($nrimages,$text,@newcmd);
				unshift @newcmd,"--keep-raw";
				&run_gphoto2_capture_target(0,$text,@newcmd);
			}
		}
	} else {
		&run_gphoto2_capture_target($nrimages,$text,@cmd);
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
	run_gphoto2_capture_formats(1,"simple capture and download","--capture-image-and-download");
	run_gphoto2_capture_formats(5,"simple capture and download","--capture-image-and-download","-F 5","-I 3");
}
