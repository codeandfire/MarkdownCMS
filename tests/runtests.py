#!/bin/python3

import json
import textwrap
import subprocess
from pathlib import Path
from collections import namedtuple
from difflib import SequenceMatcher

def display(text, textwidth=70, label=None, labelwidth=10, marks=None, markchar='^'):
    """
    label          text ......................................
                    ^  ^     ^   ^^^^^^^^^^     ^^^^^^   marks
                   ...........................................
                       ^^^^^^^^     ^^^^^^^^^^     ^^^ ^^    ^
                   ...........................................
                   ^^^^^^^   ^^^^         ^^^^^^       ^^^^^^^
                   ...........................................
                               ^^^^^^^^^^^^^^^^^^^^^^^^^    ^^

    | ----------- || --------------------------------------- |
      labelwidth                    textwidth
    """

    def nspaces(n):
        return ''.join(' ' for _ in range(n))

    if label is not None:
        assert labelwidth >= len(label)
    else:
        label = ''
        labelwidth = 0
    if marks is not None:
        assert len(text) == len(marks)    # as many marks as number of characters in text
    if text == '':
        text = '<blank>'
        marks = None

    text = label + nspaces(labelwidth - len(label)) + text
    text = textwrap.wrap(
        text,
        width=(labelwidth + textwidth),
        initial_indent='',
        subsequent_indent=nspaces(labelwidth),
        drop_whitespace=False)
    if marks is not None:
        marks = ''.join([markchar if mark else ' ' for mark in marks])
        marks = textwrap.wrap(
            marks,
            width=(labelwidth + textwidth),
            initial_indent=nspaces(labelwidth),
            subsequent_indent=nspaces(labelwidth),
            drop_whitespace=False)
        assert len(text) == len(marks)     # number of lines of text and marks has to be the same
        for i in range(len(marks)):        # insert the marks lines in between the text lines
            text.insert(2*i + 1, marks[i]);
    for line in text:
        print(line)

matcher = SequenceMatcher()
Testcase = namedtuple('Testcase', ['stdin', 'stdout', 'stderr'])

with Path(__file__).parent.joinpath('testcases.json').open() as f:
    testcases = json.load(f)

for idx, case in enumerate(testcases):
    case = Testcase(**case)
    run_obj = subprocess.run(
        [Path(__file__).parent.parent.joinpath('a.out')],
        input=case.stdin,
        text=True,
        capture_output=True)
    if run_obj.stdout == case.stdout and run_obj.stderr == case.stderr:
        continue

    print(f'Case #{idx+1}')
    display(text=case.stdin, label='stdin', marks=None)
    for field in ['stdout', 'stderr']:
        sentA, sentB = getattr(run_obj, field), getattr(case, field)
        if sentA == sentB:
            continue
        marksA = [True for _ in range(len(sentA))]
        marksB = [True for _ in range(len(sentB))]
        matcher.set_seqs(sentA, sentB)
        for block in matcher.get_matching_blocks():
            marksA[block.a : block.a + block.size] = [False for _ in range(block.size)]
            marksB[block.b : block.b + block.size] = [False for _ in range(block.size)]
        display(text=sentA, label=field, marks=marksA)
        display(text=sentB, label='expected', marks=marksB)
    print('---')
