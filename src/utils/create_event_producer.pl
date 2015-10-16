#!/usr/bin/perl
use strict;
# Copyright (C) 2015  Beniamine, David <David@Beniamine.net>
# Author: Beniamine, David <David@Beniamine.net>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

use warnings;
use bigint;
# Usage create_event_producer taskfile_names
my $MAXFILESIZE=10000000;
my $sortargs="-k1 -nu";
my $pgsize=$ARGV[0];
shift @ARGV;
my @TYPES=("Virtual","Physical");

sub do_parse_task
{
    my @FNAMES;
    my @ADDR=({}, {});
    my @FOUTS;
    my $taskfile=$_[0];
    my $task=$_[0];
    $task=~ s/.*(task[0-9]*).*/$1/;
    print "\tCreating producers files for $task\n";
    my $cpt=0;
    my $numfiles=0;
    #Init files for Physical and virtual address
    foreach (my $num =0; $num< scalar(@TYPES); ++$num)
    {
        $FNAMES[$num]="$task-$TYPES[$num]-$numfiles.prod";
        open(my $fh, ">", $FNAMES[$num]) or die "can't open $FNAMES[$num]";
        $FOUTS[$num]=$fh;
    }
    open(FILE, "<", $taskfile) or die "can't open $taskfile";
    foreach my $line (<FILE>)
    {
        if($line=~/^A[ces]* ([^ ]*) ([^ ]*) .*/)
        {
            my $val=hex($1);
            if(!exists($ADDR[0]{$val}))
            {
                $ADDR[0]{$val}=0;
                $ADDR[1]{hex($2)}=0;
                ++$cpt;
                #If to much events flush to sub file
                if($cpt== $MAXFILESIZE)
                {
                    foreach (my $num =0; $num< scalar(@TYPES); ++$num)
                    {
                        #Output the sorted files
                        for my $key (sort {$a <=> $b} keys %{$ADDR[$num]})
                        {
                            my $fh=$FOUTS[$num];
                            print $fh "$key $task\n";
                            #Clean the hashmap
                            delete $ADDR[$num]{$key};
                        }
                        close $FOUTS[$num];
                        $cpt=0;
                        ++$numfiles;
                        $FNAMES[$num]="$task-$TYPES[$num]-$numfiles.prod";
                        open(my $fh, ">", $FNAMES[$num]) or die "can't open $FNAMES[$num]";
                        $FOUTS[$num]=$fh;
                    }
                }
            }
        }
    }
    close FILE;
    foreach (my $num =0; $num< scalar(@TYPES); ++$num)
    {
        #Finish sorting
        for my $key (sort {$a <=> $b} keys %{$ADDR[$num]})
        {
            my $fh=$FOUTS[$num];
            print $fh "$key $task\n";
            #Clean the hashmap
            delete $ADDR[$num]{$key};
        }
        #Merges subfiles
        print "\tMerging $TYPES[$num] producers for $task\n";
        system("sort $sortargs -m $task-$TYPES[$num]-* > $task-$TYPES[$num]; rm $task-$TYPES[$num]-*");
        print "\t$TYPES[$num] producers merged for $task\n";
    }
    exit;
};


sub do_end
{
    my $type=$_[0];
    print "\tSorting $type producers\n";
    system("echo $pgsize > Moca-$type");
    system("sort -n -m task*-$type >> Moca-$type; rm task*-$type");
    print "\tFinishing $type producers\n";
    system("cat Moca-$type |".' awk \'{if(ADD==$1) {ACC=ACC" "$2} else {if(ADD!="") {print ADD,ACC};ADD=$1;ACC=$2}} END {print ADD" "ACC }\''." > Moca-$type.log");
    system("rm Moca-$type");
    print "\t$type producer done\n";
    exit
}

foreach my $tf (@ARGV)
{
    unless(my $pid=fork)
    {
        do_parse_task($tf);
    }
}
#Wait for partial sorts
while (wait > -1) {};
print "Finishing Framesoc files\n";
foreach my $type (@TYPES)
{
    unless(my $pid=fork)
    {
        do_end($type);
    }
}
print "Finishing Framesoc files done\n";
while (wait > -1) {};
