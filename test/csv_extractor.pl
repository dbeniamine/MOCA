#!/usr/bin/perl
#
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

use File::Find;
use File::Basename;

sub do_extract
{
    my $file=$_[0];
    my $config=basename($_[1]);
    my $Region= "Full";
    #Read the file
    open my $fh, "<", "$file" or die "can't open $file";
    while ($line=<$fh>)
    {
        #Test if the file is divided in regions
        if( $line=~/^Region: (.*) /)
        {
            $Region="$1";
        }
        elsif($line=~/.* iterations done in (.*) s \( (.*) FPS\)/)
        {
            $exec="Simulation time [s],$1\n";
            $fps="Througput [FPS],$2\n";
        }
        elsif($line=~/(Region Info),.*|(Event),.*|(Metric),.*/)
        {
            #extract csv part of the file
            #The name of the csv depends of the first word
            my $csvname=$1 eq "" ? ($2 eq "" ? $3 : $2 ): $1;
            if($line=~/$csvname,core.*,core.*/)
            {
                #Openmp stuff get out of there
                while($line=<$fh>)
                {
                    if($line=~/RDTSC .*,(.*),(.*),(.*),(.*)$/)
                    {
                        #Here we have the real openmp perfmon RDTSC Runtime
                        #We need it to fix likwid bug
                        $RDTSC[0]=$1;
                        $RDTSC[1]=$2;
                        $RDTSC[2]=$3;
                        $RDTSC[3]=$4;
                    }
                    last if $line=~/^\s*$/;
                }

            }
            else
            {
                #Rename without space
                $csvname=~s/\s/_/g;
                print("\t->$file.$csvname.csv\n");
                #First line to be printed only if the file doesn't exist
                my $firstline="";
                #For perfmon files : append data
                unless( -e "$file.$csvname.csv")
                {
                    if( $Region=~/Full/)
                    {
                        #TODO : debuguer ici
                        $execline="$config $Region,$exec";
                        $fpsline="$config $Region,$fps";
                    }else
                    {
                        $execline="";
                        $fpsline="";
                    }
                    $firstline="Expe,$line$execline$fpsline";
                }
                #copy the csv part
                open my $cfh, ">>", "$file.$csvname.csv" or die "can't open $csvname";
                printf $cfh "$firstline";
                while($line=<$fh>)
                {
                    if($line=~/(.*) STAT(.*)/)
                    {
                        #remove the STAT word to be able to compare openm with
                        #other stuffs
                        $line="$1$2\n";
                        if($line=~/RDTSC/ && ! $Region=~/Full/)
                        {
                            #We are in an openmp run and this line is broken,
                            #So we compute manually statistics from the RDTSC
                            #line previously saved
                            $max=$RDTSC[0];
                            $min=$RDTSC[0];
                            $sum=0;
                            foreach $r (@RDTSC)
                            {
                                if($r > $max)
                                {
                                    $max=$r;
                                }
                                elsif($r < $min)
                                {
                                    $min=$r;
                                }
                                $sum+=$r;
                            }
                            $mean=$sum/(scalar(@RDTSC));
                            $line="Runtime (RDTSC) [s],$sum,$max,$min,$mean\n";
                            print $line;

                        }
                    }
                    last if $line=~/^\s*$/;
                    printf $cfh "$config $Region,$line";
                }
                close $cfh;
            }
        }
    }
    close $fh;
}
#find the run* files
sub wanted
{
    if($_=~/^run[0-9]*$/)
    {
        system(cd "$File::Find::dir");
        print("$File::Find::name\n");
        do_extract("$_", "$File::Find::dir");
    }

}
File::Find::find({wanted => \&wanted}, '.');
