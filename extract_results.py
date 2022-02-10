#!/usr/bin/env python

import sys
import re

for line in sys.stdin:
    m = re.search("Timer\((.*)\): <t> = (.*), std", line)
    print(m.group(1) + ", " + m.group(2))