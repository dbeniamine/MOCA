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

sub Time{
    return $_{'Time'};
}

sub printAcc{
    my $a=$_[0];
    my $shared=$_[1];

    print FOUT "$a->{'Virt'},$a->{'Phy'},$a->{'Read'},$a->{'Write'},$a->{'CPU'},$a->{'Start'},$a->{'End'},$a->{'Task'},$shared\n";
}

my $input   = "Moca-full-trace.csv";
my $output  = "Moca-sharing.csv";
my $verbose;

my $result = GetOptions ("input=s" => \$input,
                    "output=s"   => \$output,
                    "verbose"  => \$verbose);
my %PAGES;
# my $FIN;
# my $FOUT;
my $line;
my $cpt=0;

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
    $page=~ s/...$/000/g;

    # Add it to the page
    if( !exists $PAGES{$page}){
        $PAGES{$page} = [];
    }
    push $PAGES{$page}, \%ACCESS;
    ++$cpt;
}
close FIN;
if($verbose){print "$cpt accesses parsed from $input\n";}
$cpt=0;


open FOUT, ">",$output  or die "can't open $output";
print FOUT "$head";

# Compute sharing:
# We consider every accesses to a page as a list of arrival and leave
# Each event create a new access
foreach my $page (keys %PAGES){
    if($verbose){print "Computing sharing for page $page\n";}
    # Generate list of arrival and ends
    my @Starts;
    my @Ends;
    for my $i(0 ..  $#{$PAGES{$page}}){
        @Starts[$i]={'Time'=>$PAGES{$page}[$i]{'Start'},'Id'=>$i};
        @Ends[$i]={'Time'=>$PAGES{$page}[$i]{'End'},'Id'=>$i};
    }
    # Sort both lists
    @Starts=sort {Time($a) <=> Time($b)} @Starts;
    @Ends=sort {Time($a) <=> Time($b)} @Ends;

    # Compute intersections
    my @ACCESSES;
    my $cur=@Starts[0]->{'Time'};
    my $idS=1;
    my $idE=0;
    my $old;
    my @CURIDS;
    push @CURIDS,$Starts[0]->{'Id'};

    while($idE < $#Ends){
        $old=$cur;
        if($idS <$#Starts && $Starts[$idS] <= $Ends[$idE]){
            # Arrival
            $cur=$Starts[$idS]->{'Time'};
            # Save current access
            if($#CURIDS > 0){
                push @ACCESSES, {'Start'=>$old,'End'=>$cur,'IDS'=>[@CURIDS],};
            }
            # Add id to the current access
            push @CURIDS,$Starts[$idS]->{'Id'};
            ++$idS;
        }else{
            # Leave
            $cur=$Ends[$idE]->{'Time'};
            # Save current access
            push @ACCESSES, {'Start'=>$old,'End'=>$cur,'IDS'=>[@CURIDS],};
            # Remove id from the current access
            my $index = 0;
            $index++ until $CURIDS[$index] eq $Ends[$idE]->{'Id'};
            splice(@CURIDS, $index, 1);
            ++$idE;
        }
    }

    # print the actual list of accesses
    for my $accid (0 .. $#ACCESSES){
        my $shared=(scalar(@{$ACCESSES[$accid]->{'IDS'}}) > 1)?"1":"0";
        foreach my $id (@{$ACCESSES[$accid]->{'IDS'}}){
            printAcc($PAGES{$page}[$id],$shared);
        }
    }
}
close FOUT;
if($verbose){print "$cpt shared accesses detected \n";}
