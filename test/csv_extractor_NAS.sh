#!/bin/bash

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

echo "Bench,Class,Type,Runid,Time" > results.csv
grep 'Time in seconds' -R [a-z][a-z].[A-Z] | sed -e \
    's/\([a-z][a-z]\)\.\([A-Z]\)\/run-\([0-9]*\)\/\([^-]*\).*\.log:.*=[ ]*\([0-9]*\.[0-9]*\)/\1,\2,\4,\3,\5/'\
    >> results.csv
