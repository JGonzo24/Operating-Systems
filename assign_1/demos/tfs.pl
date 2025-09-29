#!/usr/bin/perl

use Cwd;

$saveoutput=0;
$nospaces=0;
$debug=0;

if ( $ENV{"DEBUG"} ) {
   $debug=$ENV{"DEBUG"};
}

if ( $ENV{"SAVEOUTPUT"} ) {
   $saveoutput=$ENV{"SAVEOUTPUT"};
}

if ( $ENV{"JUSTONETEST"} ) {
   $justonetest=$ENV{"JUSTONETEST"};
} else {
   $justonetest=0;
}

if ( $ENV{"DIFF"} ) {
   $diff=$ENV{"DIFF"};
} else {
   $diff="diff";
}

$HOME = $ENV{"HOME"};

#print "Home is $home\n";exit;

foreach $arg ( @ARGV ) {
    if ( $arg eq "-n" || $arg eq "--nospace" ) {
	$nospaces=1;
    } elsif ( $arg eq "-s" || $arg eq "--save" ) {
	$saveoutput = 1;
    } elsif ( $arg eq "-f" || $arg eq "--force" ) {
	$force = 1;
    } elsif ( $arg eq "-v" ) {
	$debug++;
    } else {
	print STDERR "Unknown option $arg\n";
	usage();
	exit(1);
    }
}

$libdir = $ENV{"lib"};
if ( $libdir eq "" ) {
    $libdir="$HOME/CalPoly/Class/cpe453/now/Asgn/Handin/lib/asgn5";
}
$username = $ENV{"name"};
if ( $username eq "" ) {
    $username = "Submitted";
}

$refdir = $ENV{"REFDIR"};
if ( $refdir eq "" ) {
    $refdir = "$HOME/CalPoly/Class/cpe453/now/Asgn/asgn5/Soln";
}
printf "DEBUG tfs: reference dir is $refdir\n" if ( $debug );

$imagedir = $ENV{"IMAGEDIR"};
if ( $imagedir eq "" ) {
    $imagedir = "$libdir/Tests/Images";
}
printf "DEBUG tfs: image dir is $imagedir\n" if ( $debug );

$expdir = $ENV{"EXPDIR"};
if ( $expdir eq "" ) {
    $expdir = "$libdir/Tests/Expected";
}
printf "DEBUG tfs: expected dir is $expdir\n" if ( $debug );

# Force is only allowed if we are the owner of the expected output directory
if ( $force ) {
    $uid = getuid();
    $ownerid = (stat($expdir))[4];
    if ( $uid != $ownerid ) {
	print ERROR "force (-f) is only allowed for the owner of the Expected directory\n";
	print ERROR "      (un-setting)\n";
    }
    $force = 0;
}


if ( $saveoutput ) {
    if ( ! -d Output ) {
	mkdir "Output";
    }
}

%submitted = ( 
	       "minls"    => "./minls",
	       "minget"   => "./minget",
	       );

%reference = ( 
	"minls"   => "$refdir/minls",
	"minget"  => "$refdir/minget",
	       );

$maxlines = 10;
$maxchars = 2048;
$iolimit  = " iolimit $maxlines $maxchars ";
$timeout  = 5;
$sfile = "script.$$";

#Each test consists of:
#             a name,  cmd, arguments, diffargs, shouldfail, quiet
@tests = (
#--------------------------------------------------------------
[ "Usage test",                            "minls",  "-h",                                                                        "", 1,  0 ],
[ "unparitioned, blank",                   "minls",  "$imagedir/BlankDisk",                                                   "-bBw", 0,  0 ],
[ "unpartitioned, files",                  "minls",  "$imagedir/TestImage",                                                   "-bBw", 0,  0 ],
[ "unpartitioned, bad superblock",         "minls",  "$imagedir/RandomDisk",                                                  "-bBw", 1,  0 ],
[ "partitioned, part 0",                   "minls",  "-p 0 $imagedir/Partitioned",                                            "-bBw", 0,  0 ],
[ "partitioned, part 1",                   "minls",  "-p 1 $imagedir/Partitioned",                                            "-bBw", 0,  0 ],
[ "partitioned, part 2",                   "minls",  "-p 2 $imagedir/Partitioned",                                            "-bBw", 0,  0 ],
[ "partitioned, part 3",                   "minls",  "-p 3 $imagedir/Partitioned",                                            "-bBw", 0,  0 ],
[ "partitioned, bad part",                 "minls",  "-p 5 $imagedir/Partitioned",                                            "-bBw", 1,  0 ],
[ "partitioned, no part",                  "minls",  "$imagedir/Partitioned",                                                 "-bBw", 1,  0 ],
[ "partitioned, not minix",                "minls",  "-p 1 $imagedir/HardDisk",                                               "-bBw", 1,  0 ],
[ "subpart., good sub",                    "minls",  "-p 0 -s 0 $imagedir/HardDisk",                                          "-bBw", 0,  0 ],
[ "subpart., bad sub",                     "minls",  "-p 0 -s 1 $imagedir/HardDisk",                                          "-bBw", 1,  0 ],
#--------------------------------------------------------------
[ "unpartitioned, w/filename",             "minls",  "$imagedir/TestImage /Hello",                                            "-bBw", 0,  0 ],
[ "partitioned, w/filename, p0",           "minls",  "-p 0 $imagedir/Partitioned /Message",                                   "-bBw", 0,  0 ],
[ "partitioned, w/filename, p1",           "minls",  "-p 1 $imagedir/Partitioned /Message",                                   "-bBw", 0,  0 ],
[ "partitioned, w/filename, p2",           "minls",  "-p 2 $imagedir/Partitioned /Message",                                   "-bBw", 0,  0 ],
[ "partitioned, w/filename, p3",           "minls",  "-p 3 $imagedir/Partitioned /Message",                                   "-bBw", 0,  0 ],
[ "subpart., no filename s0",              "minls",  "-p 0 -s 0 $imagedir/HardDisk",                                          "-bBw", 0,  0 ],
[ "subpart., no filename s2",              "minls",  "-p 0 -s 2 $imagedir/HardDisk",                                          "-bBw", 0,  0 ],
[ "subpart., w/filename",                  "minls",  "-p 0 -s 2 $imagedir/HardDisk /home/pnico/Message",                      "-bBw", 0,  0 ],
[ "subpart., w/bad filename",              "minls",  "-p 0 -s 0 $imagedir/HardDisk /home/pnico/Message",                      "-bBw", 1,  0 ],

#--------------directories and files of various depths-------------------------
[ "directories",                           "minls",  "$imagedir/Files /Files",                                                "-bBw", 0,  0 ],
[ "directories",                           "minls",  "$imagedir/Files /Files/0000_Zones",                                     "-bBw", 0,  0 ],
[ "directories",                           "minls",  "$imagedir/Files /Files/0000_Zone",                                      "-bBw", 0,  0 ],
[ "directories",                           "minls",  "$imagedir/Files /Files/0007_Zones",                                     "-bBw", 0,  0 ],
[ "directories",                           "minls",  "$imagedir/Files /Files/0010_Zones",                                     "-bBw", 0,  0 ],
[ "directories",                           "minls",  "$imagedir/Files /Files/1032_Zones",                                     "-bBw", 0,  0 ],
[ "directories",                           "minls",  "$imagedir/Files /DeepPaths/a/b/c/d/e/i/j/k/Files",                      "-bBw", 0,  0 ],
[ "directories",                           "minls",  "$imagedir/Files /DeepPaths/a/b/c/d/e/i/j/k/Files/Message",              "-bBw", 0,  0 ],
[ "multi-zone directory",                  "minls",  "$imagedir/BigDirectories",                                              "-bBw", 0,  0 ],
[ "multi-zone directory (file)",           "minls",  "$imagedir/BigDirectories LastFile",                                     "-bBw", 0,  0 ],
[ "indirect directory",                    "minls",  "$imagedir/BigIndirectDirs Level1/Level2",                       	      "-bBw", 0,  0 ],
[ "indirect directory (file)",             "minls",  "$imagedir/BigIndirectDirs Level1/Level2/LastFile",              	      "-bBw", 0,  0 ],
#--------------ignoring deleted files--------------------------------
[ "directories-deleted",                   "minls",  "$imagedir/Files /Deleted",                                              "-bBw", 0,  0 ],
[ "directories-ok file",                   "minls",  "$imagedir/Files /Deleted/File-10",                                      "-bBw", 0,  0 ],
[ "directories-deleted file",              "minls",  "$imagedir/Files /Deleted/File-05",                                      "-bBw", 1,  0 ],
#--------------non-4096k blocks---------------------------------------
[ "1k blocks. dir.",                       "minls",  "$imagedir/SmallBlocks--1k",                                             "-bBw", 0,  0 ],
[ "1k blocks. file.",                       "minls",  "$imagedir/SmallBlocks--1k DirTwo/FileTwo ",                            "-bBw", 0,  0 ],

#--------------non-blocksize zones------------------------------------
[ "16k zones. dir.",                       "minls",  "$imagedir/BigZones-16k",                                                "-bBw", 0,  0 ],
[ "16k zones. file.",                      "minls",  "$imagedir/BigZones-16k /DirOne/FileTwo",                                "-bBw", 0,  0 ],
[ "64k zones. dir",                        "minls",  "$imagedir/ReallyBigZones-64k",                                          "-bBw", 0,  0 ],
[ "64k zones. file",                       "minls",  "$imagedir/ReallyBigZones-64k DirTwo/FileOne",                           "-bBw", 0,  0 ],
[ "64k zones. longpath",                   "minls",  "$imagedir/ReallyBigZones-64k /./.././../DirTwo/FileOne",                "-bBw", 0,  0 ],

#--------------Permissions------------------------------------
[ "Permissions 777",                      "minls", "$imagedir/Files /Permissions/777",                         	             "-bBw", 0,  0 ],
[ "Permissions 755",                      "minls", "$imagedir/Files /Permissions/755",                         	             "-bBw", 0,  0 ],
[ "Permissions 753",                      "minls", "$imagedir/Files /Permissions/753",                         	             "-bBw", 0,  0 ],
[ "Permissions 357",                      "minls", "$imagedir/Files /Permissions/357",                         	             "-bBw", 0,  0 ],
[ "Permissions 000",                      "minls", "$imagedir/Files /Permissions/000",                                       "-bBw", 0,  0 ],

#===========================now minget=================================
[ "unpartitioned, w/filename",             "minget", "$imagedir/TestImage /Hello",                                            "-bBw", 0,  0 ],
[ "partitioned, w/filename, p0",           "minget", "-p 0 $imagedir/Partitioned /Message",                                   "-bBw", 0,  0 ],
[ "partitioned, w/filename, p1",           "minget", "-p 1 $imagedir/Partitioned /Message",                                   "-bBw", 0,  0 ],
[ "partitioned, w/filename, p2",           "minget", "-p 2 $imagedir/Partitioned /Message",                                   "-bBw", 0,  0 ],
[ "partitioned, w/filename, p3",           "minget", "-p 3 $imagedir/Partitioned /Message",                                   "-bBw", 0,  0 ],
[ "subpart., no filename s0",              "minget", "-p 0 -s 0 $imagedir/HardDisk",                                          "-bBw", 1,  0 ],
[ "subpart., no filename s2",              "minget", "-p 0 -s 2 $imagedir/HardDisk",                                          "-bBw", 1,  0 ],
[ "subpart., w/filename",                  "minget", "-p 0 -s 2 $imagedir/HardDisk /home/pnico/Message",                      "-bBw", 0,  0 ],
[ "subpart., w/bad filename",              "minget", "-p 0 -s 0 $imagedir/HardDisk /home/pnico/Message",                      "-bBw", 1,  0 ],
[ "unpartitioned, w/filename",             "minget", "$imagedir/TestImage /Hello /dev/stdout",                                "-bBw", 0,  0 ],
[ "partitioned, w/filename, p0",           "minget", "-p 0 $imagedir/Partitioned /Message /dev/stdout",                       "-bBw", 0,  0 ],
[ "partitioned, w/filename, p1",           "minget", "-p 1 $imagedir/Partitioned /Message /dev/stdout",                       "-bBw", 0,  0 ],
[ "partitioned, w/filename, p2",           "minget", "-p 2 $imagedir/Partitioned /Message /dev/stdout",                       "-bBw", 0,  0 ],
[ "partitioned, w/filename, p3",           "minget", "-p 3 $imagedir/Partitioned /Message /dev/stdout",                       "-bBw", 0,  0 ],
[ "subpart., no filename s0",              "minget", "-p 0 -s 0 $imagedir/HardDisk /dev/stdout",                              "-bBw", 1,  0 ],
[ "subpart., no filename s2",              "minget", "-p 0 -s 2 $imagedir/HardDisk /dev/stdout",                              "-bBw", 1,  0 ],
[ "subpart., w/filename",                  "minget", "-p 0 -s 2 $imagedir/HardDisk /home/pnico/Message /dev/stdout",          "-bBw", 0,  0 ],
[ "subpart., w/bad filename",              "minget", "-p 0 -s 0 $imagedir/HardDisk /home/pnico/Message /dev/stdout",          "-bBw", 1,  0 ],

#--------------directories and files of various depths-------------------------
[ "directories",                           "minget", "$imagedir/Files /Files",                                                "-bBw", 1,  0 ],
[ "directories",                           "minget", "$imagedir/Files /DeepPaths/Distraction",                                "-bBw", 1,  0 ],
[ "directories",                           "minget", "$imagedir/Files /DeepPaths/a/b/c/d/e/i/j/k/Files",                      "-bBw", 1,  0 ],
[ "directories",                           "minget", "$imagedir/Files /DeepPaths/a/b/c/d/e/i/j/k/Files/Message",              "-bBw", 0,  0 ],
[ "directories",                           "minget", "$imagedir/Files /Files /dev/stdout",                                    "-bBw", 1,  0 ],
[ "directories",                           "minget", "$imagedir/Files /DeepPaths/Distraction /dev/stdout",                    "-bBw", 1,  0 ],
[ "directories",                           "minget", "$imagedir/Files /DeepPaths/a/b/c/d/e/i/j/k/Files /dev/stdout",          "-bBw", 1,  0 ],
[ "directories",                           "minget", "$imagedir/Files /DeepPaths/a/b/c/d/e/i/j/k/Files/Message /dev/stdout",  "-bBw", 0,  0 ],
[ "multi-zone directory (file)",           "minget",  "$imagedir/BigDirectories LastFile",                                    "-bBw", 0,  0 ],
[ "multi-zone directory (file)",           "minget",  "$imagedir/BigDirectories LastFile /dev/stdout",                        "-bBw", 0,  0 ],
[ "indirect directory (file)",             "minget",  "$imagedir/BigIndirectDirs Level1/Level2/LastFile",              	      "-bBw", 0,  0 ],
[ "indirect directory (file)",             "minget",  "$imagedir/BigIndirectDirs Level1/Level2/LastFile /dev/stdout", 	      "-bBw", 0,  0 ],
#--------------ignoring deleted files--------------------------------
[ "directories-deleted",                   "minget", "$imagedir/Files /Deleted",                                              "-bBw", 1,  0 ],
[ "directories-ok file",                   "minget", "$imagedir/Files /Deleted/File-10",                                      "-bBw", 0,  0 ],
[ "directories-deleted file",              "minget", "$imagedir/Files /Deleted/File-05",                                      "-bBw", 1,  0 ],
[ "directories-deleted",                   "minget", "$imagedir/Files /Deleted /dev/stdout",                                  "-bBw", 1,  0 ],
[ "directories-ok file",                   "minget", "$imagedir/Files /Deleted/File-10 /dev/stdout",                          "-bBw", 0,  0 ],
[ "directories-deleted file",              "minget", "$imagedir/Files /Deleted/File-05 /dev/stdout",                          "-bBw", 1,  0 ],
#--------------non-4096k blocks---------------------------------------
[ "1k blocks. dir.",                       "minls",  "$imagedir/SmallBlocks--1k",                                             "-bBw", 0,  0 ],
[ "1k blocks. file.",                      "minls",  "$imagedir/SmallBlocks--1k DirTwo/FileTwo ",                             "-bBw", 0,  0 ],

#--------------non-blocksize zones------------------------------------
[ "16k zones. dir.",                       "minls",  "$imagedir/BigZones-16k",                                                "-bBw", 0,  0 ],
[ "16k zones. file.",                      "minls",  "$imagedir/BigZones-16k /DirOne/FileTwo",                                "-bBw", 0,  0 ],
[ "64k zones. dir",                        "minls",  "$imagedir/ReallyBigZones-64k",                                          "-bBw", 0,  0 ],
[ "64k zones. file",                       "minls",  "$imagedir/ReallyBigZones-64k DirTwo/FileOne",                           "-bBw", 0,  0 ],
[ "64k zones. longpath",                   "minls",  "$imagedir/ReallyBigZones-64k /./.././../DirTwo/FileOne",                "-bBw", 0,  0 ],

#--------------files of various sizes--------------------------------
[ "Files size tests",                      "minget", "$imagedir/Files /Files/0000_Zones",                                      "-bBw", 0,  1 ],
[ "Files size tests",                      "minget", "$imagedir/Files /Files/0000_Zone",                                       "-bBw", 0,  1 ],
[ "Files size tests",                      "minget", "$imagedir/Files /Files/0007_Zones",                                      "-bBw", 0,  1 ],
[ "Files size tests",                      "minget", "$imagedir/Files /Files/0010_Zones",                                      "-bBw", 0,  1 ],
[ "Files size tests",                      "minget", "$imagedir/Files /Files/1032_Zones",                                      "-bBw", 0,  1 ],
[ "Files size tests",                      "minget", "$imagedir/Files /Files/0000_Zones /dev/stdout",                          "-bBw", 0,  1 ],
[ "Files size tests",                      "minget", "$imagedir/Files /Files/0000_Zone /dev/stdout",                           "-bBw", 0,  1 ],
[ "Files size tests",                      "minget", "$imagedir/Files /Files/0007_Zones /dev/stdout",                          "-bBw", 0,  1 ],
[ "Files size tests",                      "minget", "$imagedir/Files /Files/0010_Zones /dev/stdout",                          "-bBw", 0,  1 ],
[ "Files size tests",                      "minget", "$imagedir/Files /Files/1032_Zones /dev/stdout",                          "-bBw", 0,  1 ],
#--------------files with holes--------------------------------------
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/nohole         /dev/stdout",                     "-bBw", 0,  1 ],
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/direct         /dev/stdout",                     "-bBw", 0,  1 ],
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/into-indirect  /dev/stdout",                     "-bBw", 0,  1 ],
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/whole-indirect /dev/stdout",                     "-bBw", 0,  1 ],
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/double         /dev/stdout",                     "-bBw", 0,  1 ],
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/nohole",                                         "-bBw", 0,  1 ],
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/direct",                                         "-bBw", 0,  1 ],
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/into-indirect",                                  "-bBw", 0,  1 ],
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/whole-indirect",                                 "-bBw", 0,  1 ],
[ "Reading files with holes",              "minget", "$imagedir/Files /Holes/double",                                         "-bBw", 0,  1 ],
#--------------various structural issues: explicit checks for bad magic numbers, file types, etc--------------------------------------
[ "minls:  partition table with bad type", "minls",  "-p 0 $imagedir/BrokenStuff",                                            "-bBw", 1,  0 ],
[ "minget: partition table with bad type", "minget", "-p 0 $imagedir/BrokenStuff README /dev/stdout",                         "-bBw", 1,  0 ],
[ "minls:  superblock with bad magic",     "minls",  "-p 1 $imagedir/BrokenStuff",                                            "-bBw", 1,  0 ],
[ "minget: superblock with bad magic",     "minget", "-p 1 $imagedir/BrokenStuff README /dev/stdout",                         "-bBw", 1,  0 ],
[ "minls:  partition table with bad magic","minls",  "-p 2 -s 0 $imagedir/BrokenStuff",                                       "-bBw", 1,  0 ],
[ "minget: partition table with bad magic","minget", "-p 2 -s 0 $imagedir/BrokenStuff README /dev/stdout",                    "-bBw", 1,  0 ],
[ "minls:  trying to traverse a file",     "minls",  "$imagedir/WeirdStuff NotADir/This_file_does_not_exist",                 "-bBw", 1,  0 ],
[ "minget: trying to traverse a file",     "minget", "$imagedir/WeirdStuff NotADir/This_file_does_not_exist /dev/stdout",     "-bBw", 1,  0 ],
[ "minget: trying to get a symlink",       "minget", "$imagedir/WeirdStuff SymLink",                                          "-bBw", 1,  0 ],
[ "minget: trying to get a directory",     "minget", "$imagedir/TestImage src /dev/stdout",                                   "-bBw", 1,  0 ],
#--------------various structural issues: explicit checks for bad magic numbers, file types, etc--------------------------------------
[ "minget: indirect block vs. zone",       "minget", "$imagedir/indirectblock tryme /dev/stdout",                              "-bBw", 0,  1 ],
[ "minls: long file name",       "minls", "$imagedir/indirectblock ThisNameHasSixtyCharactersItReallyDoesThatsALotReallyItIsBoo",  "-bBw", 0,  0 ],
[ "minls: long file name, partial match",       "minls", "$imagedir/indirectblock ThisNameHasSixtyCharactersItReallyDoesThatsALotReallyItIsBooTooLong",  "-bBw", 1,  0 ],
#[ "",                           ""      ,  ""                         , 0 ],
#	  [ "", 	"", "" ,],
#	  [ "", 	"", "" ],
#	  [ "", 	"", "" ],
#	  [ "", 	"", "" ],
);


$logfile   = "logminfs.$$";
$difffile  = "diffminfs.$$";
$testfile  = "testminfs.$$";
$errfile  = "errminfs.$$";
$errors    = 0;
#$format = "   %-28s %-6s %-40s %4s\n";
#$format = "   %s\n      %-28s %-6s %-40s\n";
$format = "%-59s %8s\n       Cmd: %s %-40s \n";

$line="=======================================================================";
printf("   $line\n  ");
printf($format,"Test","Status","Prog","Arguments");
printf("   $line\n");
$errors += dotests(@tests);
printf("   $line\n");

$total=scalar(@tests);
$passed=$total-$errors;

if ( $errors == 0 ) {
    printf("Success. (%d/%d passed)\n",$passed,$total);
} else {
    printf("Failure. (%d/%d passed)\n",$passed,$total);
}

if ( $errors ) {
    printf("\n");
    printf("Actual ouput from tests and detailed results are below.\n");
    printf("\n");
    system("cat $logfile");
    unlink($logfile);
}

exit $errors;


sub dotests {
    local(@testarray) = @_;
    local($errors) = 0;
    $testnum=1;
    foreach $t ( @testarray ) {
	($name,$cmd,$args,$diffargs,$shouldfail,$quiet) = (@$t);
	if ( $nospaces ) {
	    # remove spaces between -p, -s, and the number
	    $_ = $args;
	    s/-p\s+(\d+)/-p$1/g;
	    s/-s\s+(\d+)/-s$1/g;
	    $args = $_;
	}
	unlink($testfile);
	unlink($errfile);
	unlink($difffile);
	if ( !$justonetest || ($testnum==$justonetest) ) {
	    $errors += onetest($testnum,$name,$cmd,$args,$diffargs,$shouldfail,$quiet);
	}
	$testnum++;
    }
    return $errors;
}

sub onetest {
    local($testnum,$name,$cmd,$args,$diffargs,$shouldfail,$quiet) = @_;
    local($oops)=0;

    if ( ! -x $submitted{$cmd} ) {
	# announce the test to the logfile
	open  (OUT,">> $logfile");
	print OUT "\n\n-----------------\n$testnum) $name:\n     $cmd " .
	    "$args \n-----------------\n";
	print OUT "Unable to locate $submitted{$cmd}\n";
	close (OUT);
	$failed = 1;
    } else {
	#WAS	$command="scriptwrap -f/dev/null $HOME/bin/timelimit $timeout " . $submitted{$cmd} . " $args  > $testfile";

#WORKS $command="/home/pnico/CalPoly/Class/cpe453/2018-02/Asgn/Handin/GradingTest/asgn5/awtam/Grading/scriptwrap -f/dev/null $HOME/bin/timelimit $timeout " . $submitted{$cmd} . " $args  > $testfile";
#WILL BE	$command="scriptwrap2 -f $testfile -q -t $timeout " . $submitted{$cmd} . " $args  ";
#
#
#This is what it should be, but I'll have to regenerate the reference files
#because of scriptwrap
#
$command="scriptwrap2 -f $testfile -o $testfile -r $errfile -q -t $timeout " . $submitted{$cmd} . " $args  ";

	$chatter = $debug;	# PLN
	if ( $chatter ) {
	    print "\n\n";
	    print "CL: '$command'\n";
	    print "\n\n";
	}
	if ( $saveoutput ) {
	    open (CMD,">Output/cmd.$testnum");
	    print CMD "TST: \"$command\"\n";
	    close(CMD);
	}

#	$status = system("$command");
	if ( 0 == ($pid = fork()) ) {
	    # Child
	    exec $command;
	    exit 1;		# nobody expects you to come back
	}
	wait();
	$status = $? >> 8;
    
	$failed = 0;

	if ( ! $status ) {	    # it exited zero.  A good sign
	    if ( $shouldfail ) { # unless it was supposed to fail...
		$failed = 1;
		open  (OUT,">> $logfile");
		print OUT "\n\n-----------------\n$testnum) $name:\n     $cmd " .
		    "$args \n-----------------\n";
		print OUT " This test was expected to fail, but reports success.\n";
		print OUT " Perhaps a bad exit status or no error message?\n";
		close (OUT);
	    } else {
		# now see if it got the right output.
		$reffile = "$expdir/$testnum";
		if ( ! -f "$reffile" ) {
		    # create the reference file
		    $cl="scriptwrap2 -f $reffile -o $reffile -r $reffile -q -t $timeout " . $reference{$cmd} . " $args  ";
#WORKS		    $cl="/home/pnico/CalPoly/Class/cpe453/2018-02/Asgn/Handin/GradingTest/asgn5/awtam/Grading/scriptwrap  -f/dev/null " . $reference{$cmd} . " $args > $reffile";
#print("\nCommand: \"$cl\"\n");
		    if ( 0 == ($pid = fork()) ) {
			# Child
			exec $cl;
			exit 1;		# nobody expects you to come back
		    }
		    wait();
		    $status = $? >> 8;
		    if ( $saveoutput ) {
			open (CMD,">> Output/RefCmd.$testnum");
			print CMD "CMD: \"$cl\"\n";
			close(CMD);
		    }
		}
		$status=system("$diff $diffargs $reffile $testfile > $difffile");
		
		if ( $status ) {# if different (and unexpected)
		    $failed = 1;		    
		    open  (OUT,">> $logfile");
		    print OUT "\n\n-----------------\n$testnum) $name:\n     $cmd " .
			"$args \n-----------------\n";
		    open  (OUT,">> $logfile");
		    print OUT "Test program output:\n";
		    close (OUT);
		    if ( ! $quiet ) {
			system("cat -v $testfile >> $logfile");
		    } else {
			open  (OUT,">> $logfile");
			print OUT " --- Output limited to 10 lines. (There would be too much.) ---\n";
			close (OUT);
			system("cat -v $testfile | $iolimit >> $logfile 2>&1");
		    }
		    open  (OUT,">> $logfile");
		    print OUT "Diffs: ( < Reference > Testfile ):\n";
		    close (OUT);
		    if ( ! $quiet ) {
			system("cat -v $difffile >> $logfile");
		    } else {
			open  (OUT,">> $logfile");
			print OUT " --- Output limited to 10 lines. (There would be too much.) ---\n";
			close (OUT);
			system("cat -v $difffile | $iolimit >> $logfile 2>&1");
		    }
		}
	    }
	} else {
	    # PLN
	    #	open  (OUT,">> $logfile");
#		print OUT "\n\n-----------------\n$testnum) $name:\n     $cmd " .
#		    "$args \n-----------------\n";
#		print OUT "shouldfail is $shouldfail\n";
#		if ( -f $errfile ) {
#		    print OUT "$errfile doesn't exist\n";
#		} elsif ( -z $errfile ) {
#		    print OUT "-z $errfile is true\n";
#		} else {
#		    print OUT "-z $errfile is false\n";
#		}
#		close (OUT);
#		system("ls -l $errfile >> $logfile 2>&1");
	    # NLP

	    # program execution exited nonzero (failure)
	    if ( ! $shouldfail ) { 
		$failed = 1;
		open  (OUT,">> $logfile");
		print OUT "\n\n-----------------\n$testnum) $name:\n     $cmd " .
		    "$args \n-----------------\n";
		print OUT "Program exited with bad status: $status\n\n";
		print OUT "Test program output:\n";
		close (OUT);
		if ( ! $quiet ) {
		    system("cat -v $testfile >> $logfile");
		} else {
		    open  (OUT,">> $logfile");
		    print OUT " --- Output limited to 10 lines. (There would be too much.) ---\n";
		    close (OUT);
		    system("cat -v $testfile | $iolimit >> $logfile 2>&1");
		}
		close (OUT);
	    } elsif ( -z $errfile ) {
		# there should be an error message if it was expected to fail and did
		$failed = 1;
		open  (OUT,">> $logfile");
		print OUT "\n\n-----------------\n$testnum) $name:\n     $cmd " .
		    "$args \n-----------------\n";
		print OUT "Program was expected to fail, but produced no stderr output\n\n";
		print OUT "Test program output:\n";
		close (OUT);
		if ( ! $quiet ) {
		    system("cat -v $testfile >> $logfile");
		} else {
		    open  (OUT,">> $logfile");
		    print OUT " --- Output limited to 10 lines. (There would be too much.) ---\n";
		    close (OUT);
		    system("cat -v $testfile | $iolimit >> $logfile 2>&1");
		}
		close (OUT);
	    }
	}

        # report error output if any
        if ( $failed && -f $errfile && ! -z $errfile ) {
	    open  (OUT,">> $logfile");
	    print OUT "Submitted program's stderr:\n";
	    close(OUT);
	    if ( ! $quiet ) {
		system("cat -v $errfile >> $logfile");
	    } else {
		open  (OUT,">> $logfile");
		print OUT " --- Output limited to 10 lines. (There would be too much.) ---\n";
		close (OUT);
		system("cat -v $errfile | $iolimit >> $logfile 2>&1");
	    }
	    close (OUT);
	}


        if ( $saveoutput ) {
	    rename($testfile,"Output/test.$testnum.out");
	    rename($errfile,"Output/test.$testnum.err");
	} else {
	    unlink($testfile);
	    unlink($errfile);
	}
	unlink($difffile);

	if ( $chatter ) {
	    open  (OUT,">> $logfile");
	    print OUT "shouldfail is $shouldfail and failed is $failed\n";
	    if ( $shouldfail ) {
		$failed = $failed?0:1;
		print OUT "\tNow, it's $failed\n";
	    }
	    close(OUT);
	}
    }

    if ( $failed ) {
	$oops ++;
    }
    printf("%3d -- ",$testnum);
    $_ = $args;
    s/(\s)?$imagedir\//$1/;
    $args = $_;
#	printf($format,$name,$cmd,$args,$status?"FAILED":"ok");
    printf($format,$name,$failed?"FAILED.":"ok.",$cmd,$args);
#	printf($format,$status?"FAILED":"ok",$name,$cmd,$args);
#	last; 			# PLN
    return $oops;
}

sub usage {
    print STDERR "usage: tfs.pl [ -v ][ -n | --nospace ] [ -s | --save ] [ -f | --force ]\n";
    print STDERR "       -v --- increase verbosity\n";
    print STDERR "       -n --- don't put spaces between switches and arguments\n";
    print STDERR "       -s --- save test outputs for later comparison\n";
    print STDERR "       -f --- force recreation of the reference outputs (admin only)\n";
}
