nbdsh -c - <<'EOF'
from subprocess import *

h.connect_uri("nbd://localhost")
bootsect = h.pread(512, 0)
p = Popen("hexdump -C", shell=True, stdin=PIPE)
p.stdin.write(bootsect)
EOF
