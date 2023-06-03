#!/bin/python3

import sys
import yaml
import subprocess
import unicodedata
from pathlib import Path
from collections import namedtuple
from difflib import SequenceMatcher

debug = '-debug' in sys.argv[1:]
spaceout = '-spaceout' in sys.argv[1:]

def _wrap(text, width, initial_indent='', subsequent_indent=''):
    """A text wrapping function.

    The textwrap.wrap() function in the Python standard library is meant for text wrapping
    but does not do precisely what we want. In particular, it breaks the line on
    whitespace which messes up the positions of the marks. For example:

        aaaaaaaaaabbbbbbbbbbbbbbbbb
        ^^^^^^^^^^

        bbbbbbbbbbaaaaaaaaaaaaaa
                  ^^^^^^^^^^^^^^

    The a's are meant to be marked, as shown above. But with textwrap.wrap() we will get:

        aaaaaaaaaabbbbbbbbbbbbbbbbb
        ^^^^^^^^^^

        bbbbbbbbbbaaaaaaaaaaaaaa
        ^^^^^^^^^^^^^^

    i.e. encountering continuous whitespace after the marks for the a's on the first line,
    a linebreak is issued and further whitespace is stripped due to which the marks for
    the a's on the second line are brought to the front of the line resulting in incorrect
    marking.

    (Basically textwrap.wrap() is based on standard rules for word wrapping, which is not
    what we want.)

    This function, however, is based on the design of textwrap.wrap() (in particular its
    initial_indent and subsequent_indent arguments behave the same as that function).
    """

    assert len(initial_indent) < width
    assert len(subsequent_indent) < width

    lines = []
    while len(text) > 0:
        indent = initial_indent if len(lines) == 0 else subsequent_indent
        lines.append(indent + text[: width - len(indent)])
        text = text[width - len(indent): ]
    return lines

def display(
        text, textwidth=90,
        label=None, labelwidth=20,
        marks=None, markchar='^',
        showblanks=True,
        spaceout=True, spacechar=unicodedata.lookup('BLACK CIRCLE')):
    """
    Display a piece of text, with an optional label and marks, as follows:

    <label>        ........................................... <text>
                    ^  ^     ^   ^^^^^^^^^^     ^^^^^^         <marks>
                   ........................................... <text>
                       ^^^^^^^^     ^^^^^^^^^^     ^^^ ^^    ^ <marks>
                   ........................................... <text>
                   ^^^^^^^   ^^^^         ^^^^^^       ^^^^^^^ <marks>
                   ........................................... <text>
                               ^^^^^^^^^^^^^^^^^^^^^^^^^    ^^ <marks>

    | ----------- || ---------------------------------------- |
      labelwidth                    textwidth

    With spaceout=True:

    <label>        . . . . . . . . . . . . . . . . . . . . . . . <text>
                     ^     ^     ^ ^ ^ ^ ^ ^ ^ ^ ^ ^ ^     ^     <marks>
                   . . . . . . . . . . . . . . . . . . . . . . . <text>
                       ^ ^ ^ ^ ^ ^             ^ ^ ^   ^ ^     ^ <marks>

    i.e. each character in the text and the marks is separated by a single space.
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
        if len(marks) == 0:     # this will mean len(text) == 0 due to the above assertion
            marks = None        # don't display any marks
        else:
            marks = [(markchar if mark else ' ') for mark in marks]

    text = list(text)           # list of characters in text
    if showblanks:
        if marks is not None:
            # marks corresponding to \t, \n or \r need an extra space after the mark
            # because they will now correspond to two characters (\\t, \\n, \\r) instead
            # of one.
            marks = [
                mark + (' ' if char in ('\t', '\n', '\r') else '')
                for mark, char in zip(marks, text)
            ]
        for old, new in zip(
                (' ', '\t', '\n', '\r'), (spacechar, '\\t', '\\n', '\\r')):
            text = [new if char == old else char for char in text]
    text = (' ' if spaceout else '').join(text)
    if marks is not None:
        marks = (' ' if spaceout else '').join(marks)

    text = _wrap(
        label + nspaces(labelwidth - len(label)) + text,
        labelwidth + textwidth,
        initial_indent=nspaces(0),
        subsequent_indent=nspaces(labelwidth))

    if marks is not None:
        marks = _wrap(
            marks,
            labelwidth + textwidth,
            initial_indent=nspaces(labelwidth),
            subsequent_indent=nspaces(labelwidth))
        assert len(marks) == len(text)      # both must have the same number of lines
        # append the marks lines after their corresponding text lines
        text = [
            text_line + '\n' + marks_line for text_line, marks_line in zip(text, marks)]
    print('\n'.join(text))

try:                        # recompile before running tests
    subprocess.run(
        ['cc', Path(__file__).parent.parent.joinpath('translate.c')],
        text=True, check=True)
except subprocess.CalledProcessError:
    print('Compilation failed.', file=sys.stderr)
    sys.exit(1)             # exit with code 1 if compilation fails

matcher = SequenceMatcher()
Testcase = namedtuple(
    'Testcase', ['stdin', 'stdout', 'stderr'], defaults=[None, None, ''])

testfile = 'testrunner_testcases.yml' if debug else 'testcases.yml'
with Path(__file__).parent.joinpath(testfile).open() as f:
    testcases = yaml.safe_load_all(f)
    testcases = [case for doc in testcases for case in doc]     # doc = YAML document

count = 0
for idx, case in enumerate(testcases):
    case = Testcase(**case)
    run_obj = subprocess.run(
        [Path(__file__).parent.parent.joinpath('a.out')],
        input=case.stdin,
        text=True,
        capture_output=True)
    if run_obj.stdout == case.stdout and run_obj.stderr == case.stderr:
        count += 1
        continue

    print(f'Case #{idx+1}')
    display(text=case.stdin, label='stdin', marks=None, spaceout=spaceout)
    for field in ['stdout', 'stderr']:
        sentA, sentB = getattr(run_obj, field), getattr(case, field)
        if sentA == sentB:
            continue
        marksA = [True for _ in range(len(sentA))]  # initially assume all characters are marked
        marksB = [True for _ in range(len(sentB))]
        matcher.set_seqs(sentA, sentB)
        for block in matcher.get_matching_blocks():
            # the sequences belonging to matching blocks are correct and should not be marked
            marksA[block.a : block.a + block.size] = [False for _ in range(block.size)]
            marksB[block.b : block.b + block.size] = [False for _ in range(block.size)]
        display(text=sentA, label=field, marks=marksA, spaceout=spaceout)
        display(text=sentB, label='expected', marks=marksB, spaceout=spaceout)
    print('---')

print(f'{count}/{idx+1} passed.')
sys.exit(0 if count == idx+1 else 2);   # exit with code 2 if some tests fail
