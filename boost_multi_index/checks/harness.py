#!/usr/bin/env python3
"""Grade a learner's C++ answer by compiling and running it inside a test harness.

Usage: harness.py <template.cpp>   (answer on stdin)
The template contains the line `/*ANSWER*/`; the learner's text replaces it.
Markdown code fences around the answer are stripped. Exit 0 = HIT, 1 = MISS.
Compiler: clang++ -std=c++23 -O1 -I/opt/homebrew/include (override with CXX / BOOST_INC).
"""
import os, re, subprocess, sys, tempfile

def main():
    template = open(sys.argv[1], encoding='utf-8').read()
    answer = sys.stdin.read().strip()
    answer = re.sub(r'^```[a-zA-Z+]*\s*\n?', '', answer)
    answer = re.sub(r'\n?```\s*$', '', answer).strip()
    if not answer:
        print('MISS: empty answer'); return 1
    src = template.replace('/*ANSWER*/', answer)
    with tempfile.TemporaryDirectory() as tmp:
        cpp, exe = os.path.join(tmp, 'a.cpp'), os.path.join(tmp, 'a.out')
        open(cpp, 'w', encoding='utf-8').write(src)
        cxx = os.environ.get('CXX', 'clang++')
        inc = os.environ.get('BOOST_INC', '/opt/homebrew/include')
        c = subprocess.run([cxx, '-std=c++23', '-O1', '-I' + inc,
                            '-I' + os.path.dirname(os.path.abspath(sys.argv[1])), cpp, '-o', exe],
                           capture_output=True, text=True, timeout=120)
        if c.returncode != 0:
            err = [l for l in c.stderr.splitlines() if 'error' in l][:3]
            print('MISS: does not compile\n' + '\n'.join(err)); return 1
        r = subprocess.run([exe], capture_output=True, text=True, timeout=20)
        print(r.stdout.strip())
        return 0 if r.returncode == 0 else 1

if __name__ == '__main__':
    sys.exit(main())
