#!perl

use strict;
use Cwd qw(abs_path);
use File::Basename;
use File::Spec;
use Test;

my $testdir;
my $scstadmin;
my $redirect_file;
my $redirect;

BEGIN {
    $redirect_file = "/tmp/scstadmin-test-08-output.txt";
    unlink($redirect_file);
    $testdir = dirname(abs_path($0));
    my $scstadmin_pm_dir = dirname($testdir);
    my $scstadmin_dir = dirname($scstadmin_pm_dir);
    $scstadmin = File::Spec->catfile($scstadmin_dir, "scstadmin");
    unless(grep /blib/, @INC) {
	unshift(@INC, File::Spec->catdir($scstadmin_pm_dir, "lib"));
    }
    plan tests => 5;
}

use Data::Dumper;
use SCST::SCST;
use File::Temp qw/tempfile/;

sub setup {
    my $SCST = shift;

    my ($drivers, $errorString) = $SCST->drivers();
    my %drivers = map { $_ => 1 } @{$drivers};
    ok(exists($drivers{'scst_local'}));
    system("dd if=/dev/zero of=/dev/scstadmin-regression-test-vdisk bs=1M count=1 >/dev/null 2>&1");
}

sub teardown {
    unlink("/dev/scstadmin-regression-test-vdisk");
}

sub filterScstLocal {
    my $in = shift;
    my $out = shift;

    system("awk 'BEGIN { t = 0 } /^# Automatically generated by SCST Configurator v/ { \$0 = \"# Automatically generated by SCST Configurator v...\" } /^TARGET_DRIVER.*{\$/ { if (match(\$0, \"TARGET_DRIVER ([^ ]*) {\", d) && d[1] != \"scst_local\") t = 1 } /^}\$/ { if (t == 1) t = 2 } /^\$/ { if (t == 2) { t = 3 } } /^./ { if (t == 3) { t = 0 } } { if (t == 0) print }' <" . '"' . "$in" . '" >"' . "$out" . '"');
}

sub closeDevs {
    my $cmd;

    foreach my $dev (@_) {
	if ($cmd) {
	    $cmd .= " && ";
	}
	$cmd .= "$scstadmin -noprompt -handler vdisk_nullio -close_dev $dev";
    }
    return system("{ echo " . '"' . "$cmd" . '"' . "; $cmd; } $redirect");    
}

sub closeDevTest {
    my $tmpfilename1 = File::Spec->catfile(File::Spec->tmpdir(),
					   "scstadmin-test-08-$$-1");
    my $tmpfilename2 = File::Spec->catfile(File::Spec->tmpdir(),
					   "scstadmin-test-08-$$-2");
    my $rc;

    system("$scstadmin -clear_config -force -noprompt -no_lip $redirect");
    system("$scstadmin -open_dev nodev -handler vdisk_nullio -attributes dummy=1 $redirect");
    system("$scstadmin -open_dev disk0 -handler vdisk_fileio -attributes filename=/dev/scstadmin-regression-test-vdisk,read_only=1 $redirect");
    system("$scstadmin -open_dev disk1 -handler vdisk_fileio -attributes filename=/dev/scstadmin-regression-test-vdisk,nv_cache=1 $redirect");
    system("$scstadmin -driver scst_local -add_target local $redirect");
    system("$scstadmin -driver scst_local -target local " .
	   "-add_lun 0 -device nodev $redirect");
    system("$scstadmin -driver scst_local -target local -add_group ig " .
	   "$redirect");
    system("$scstadmin -driver scst_local -target local -group ig " .
	   "-add_init ini1 $redirect");
    system("$scstadmin -driver scst_local -target local -group ig " .
	   "-add_init ini2 $redirect");
    system("$scstadmin -driver scst_local -target local -group ig " .
	   "-add_lun 0 -device disk0 $redirect");
    system("$scstadmin -driver scst_local -target local -group ig " .
	   "-add_lun 1 -device disk1 $redirect");
    system("$scstadmin -write_config $tmpfilename1 >/dev/null");
    # Keep only the scst_local target driver information.
    filterScstLocal("$tmpfilename1", "$tmpfilename2");
    system("cd /sys/kernel/scst_tgt && find -ls $redirect");
    $rc = system("$scstadmin -dumpAttrs $redirect");
    ok($rc, 0);
    $rc = closeDevs("nodev");
    ok($rc, 0);
    $rc = closeDevs("disk0", "disk1");
    ok($rc, 256);
    system("$scstadmin -noprompt -driver scst_local -target local -group ig " .
	   "-rem_lun 0 -device disk0 $redirect");
    system("$scstadmin -noprompt -driver scst_local -target local -group ig " .
	   "-rem_lun 1 -device disk1 $redirect");
    $rc = closeDevs("disk0", "disk1");
    ok($rc, 0);
    if ($rc == 0) {
	unlink($tmpfilename2);
	unlink($tmpfilename1);
    }
}

my $_DEBUG_ = 0;
if ($_DEBUG_) {
    $redirect = ">>$redirect_file 2>&1";
    open(my $logfile, '>>', $redirect_file);
    select $logfile;
} else {
    $redirect = ">/dev/null";
}

my $SCST = eval { new SCST::SCST($_DEBUG_) };
die("Creation of SCST object failed") if (!defined($SCST));

setup($SCST);

closeDevTest();

teardown();
