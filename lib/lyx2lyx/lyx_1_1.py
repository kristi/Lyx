# This file is part of lyx2lyx
# -*- coding: utf-8 -*-
# Copyright (C) 2004 José Matos <jamatos@lyx.org>
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

""" Convert files to the file format generated by lyx 1.1 series, until 1.1.4"""

supported_versions = ["1.1.%d" % i for i in range(5)] + ["1.1"]
convert = [[215, []]]
revert  = []


if __name__ == "__main__":
    pass

