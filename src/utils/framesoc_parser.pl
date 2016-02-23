#!/usr/bin/perl

# Copyright (C) 2016  Beniamine, David <David@Beniamine.net>
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


use Getopt::Long; # options
use Data::Dumper; # debug
use strict;

my $prevline="";
sub printAcc($$$$){
    my $a=shift;
    my $shared=shift;
    my $start=shift;
    my $end=shift;

    my $line="$a->{'Virt'},$a->{'Phy'},$a->{'Read'},$a->{'Write'},$a->{'CPU'},$start,$end,$a->{'Task'},$shared\n";
    if($line ne $prevline){
        # Lines can be duplicated in some border case, we just have to ignore
        # them
        print FOUT $line;
    }
    $prevline=$line;
}

my $input       = "Moca-full-trace.csv";
my $output      = "Moca-framesoc.csv";
my $pagesize    = 4096;
my $verbose;
my $debug;

my $result = GetOptions ("input=s"  => \$input,
    "output=s"      => \$output,
    "pagesize=n"    => \$pagesize,
    "verbose"       => \$verbose,
    "debug"         => \$debug);
my %PAGES;
my $line;
my $cpt=0;
my $pageMask = sprintf("%x",$pagesize);
$pageMask =~ s/1//;
my $pageZeros=$pageMask;
$pageMask =~ s/0/./g;

open FIN, "<",$input  or die "can't open $input";

# Parse the input
if($verbose){print "Reading from $input\n";}

my $head=<FIN>; # Skip header
$head=~s/ //g;
$head=~ s/$/,Shared/;
while ($line=<FIN>){
    # Parse access
    $line=~ s/ //g;
    $line=~ s/\R//g;
    my @FIELDS=split(',', $line);

    my %ACCESS=('Virt'  => $FIELDS[0],
        'Phy'   => $FIELDS[1],
        'Read'  => $FIELDS[2],
        'Write' => $FIELDS[3],
        'CPU'   => $FIELDS[4],
        'Start' => $FIELDS[5],
        'End'   => $FIELDS[6],
        'Task'  => $FIELDS[7],
    );

    my $page=$FIELDS[0];
    $page=~ s/$pageMask$/$pageZeros/g;

    # Add it to the page
    if( !exists $PAGES{$page}){
        $PAGES{$page} = [];
    }
    push $PAGES{$page}, \%ACCESS;
    ++$cpt;
}
my $npages=scalar keys %PAGES;
close FIN;
if($verbose){print "$cpt accesses on $npages pages parsed from $input\n";}
$cpt=0;


open FOUT, ">",$output  or die "can't open $output";
print FOUT "$head";

# Compute sharing:
# We consider every accesses to a page as a list of arrival and leave
# Each event create a new access
foreach my $page (sort keys %PAGES){
    if($debug){print "Extracting intervals for page $page\n";}
    # Generate list of arrival and ends
    my @Starts;
    my @Ends;
    for my $i(0 ..  $#{$PAGES{$page}}){
        @Starts[$i]={'Time'=>$PAGES{$page}[$i]{'Start'},'Id'=>$i};
        @Ends[$i]={'Time'=>$PAGES{$page}[$i]{'End'},'Id'=>$i};
    }
    if($debug){print "Sorting intervals for page $page\n";}
    # Sort both lists
    @Starts=sort {$a->{'Time'} <=> $b->{'Time'}} @Starts;
    @Ends=sort {$a->{'Time'} <=> $b->{'Time'}} @Ends;

    # Compute intersections
    my @ACCESSES;
    my $cur=-1;
    my $idS=0;
    my $idE=0;
    my $old=-1;
    my @CURIDS;

    if($debug){print "Computing intersections for page $page\n";}
    while($idE < scalar @Ends){
        $old=$cur;
        if($idS < scalar @Starts && $Starts[$idS]->{'Time'} <= $Ends[$idE]->{'Time'}){
            # Arrival
            $cur=$Starts[$idS]->{'Time'};
            # Save current access
            if(scalar @CURIDS > 0){
                push @ACCESSES, {'Start'=>$old,'End'=>$cur,'IDS'=>[@CURIDS],};
            }
            # Add id to the current access
            my $index = 0;
            $index++ until $index >= scalar @CURIDS or $CURIDS[$index] eq $Starts[$idS]->{'Id'};
            push @CURIDS,$Starts[$idS]->{'Id'};
            ++$idS;
        }else{
            # Leave
            $cur=$Ends[$idE]->{'Time'};
            # Save current access
            push @ACCESSES, {'Start'=>$old,'End'=>$cur,'IDS'=>[@CURIDS],};
            # Remove id from the current access
            @CURIDS = grep {$_ != $Ends[$idE]->{'Id'} } @CURIDS;
            ++$idE;
        }
    }
    if (scalar @CURIDS != 0){
        print "After all events for page $page, curids not empty: $#CURIDS: '@CURIDS'\n";
        print Dumper @ACCESSES;
        exit 1
    }

    if($debug){print "dumping accesses for page $page\n";}
    # print the actual list of accesses
    for my $accid (0 .. $#ACCESSES){
        my $shared=(scalar(@{$ACCESSES[$accid]->{'IDS'}}) > 1)?"1":"0";
        foreach my $id (@{$ACCESSES[$accid]->{'IDS'}}){
            # print "ai: $accid p: $page, i: $id, s: $ACCESSES[$accid]->{'Start'}, e: $ACCESSES[$accid]->{'End'}\n";
            printAcc($PAGES{$page}[$id],$shared,$ACCESSES[$accid]->{'Start'},
                $ACCESSES[$accid]->{'End'});
        }
    }
}
close FOUT;
#TODO: print framesoc files from here
