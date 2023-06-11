#!/bin/python3

import sys
import yaml
import difflib
import subprocess
import dataclasses
import unicodedata
from pathlib import Path

_matcher = difflib.SequenceMatcher()

def _wrap(text, width, initial_indent='', subsequent_indent=''):
    assert len(initial_indent) < width
    assert len(subsequent_indent) < width

    lines = []
    while len(text) > 0:
        indent = initial_indent if len(lines) == 0 else subsequent_indent
        lines.append(indent + text[: width - len(indent)])
        text = text[width - len(indent): ]
    return lines

def print_chardiff(text, textwidth, marks=None, label='', labelwidth=0, markchar='^', spaceout=False,
                   spacechar=unicodedata.lookup('BLACK CIRCLE')):
    def nspaces(n):
        return ''.join(' ' for _ in range(n))

    if marks is not None:
        assert len(text) == len(marks)                      # as many marks as there are characters in text

        if len(marks) == 0:                                 # due to the above assertion, no marks implies no text
            marks = None                                    # don't display marks (will be a blank line anyway)
            text = '<blank>'

    assert len(label) <= labelwidth

    if marks is not None:
        marks = [(markchar if mark else ' ') for mark in marks]
        marks = [mark + (' ' if char in ('\t', '\n', '\r') else '') for mark, char in zip(marks, text)]
        marks = (' ' if spaceout else '').join(marks)

    for old, new in ((' ', spacechar), ('\t', '\\t'), ('\n', '\\n'), ('\r', '\\r')):
        text = [new if char == old else char for char in text]
    text = (' ' if spaceout else '').join(text)

    # add the label as part of the text
    text = label + nspaces(labelwidth - len(label)) + text
    textwidth += labelwidth

    text = _wrap(text, textwidth, initial_indent=nspaces(0), subsequent_indent=nspaces(labelwidth))
    if marks is not None:
        marks = _wrap(marks, textwidth, initial_indent=nspaces(labelwidth), subsequent_indent=nspaces(labelwidth))
        assert len(marks) == len(text)                      # as many lines of marks as lines of text
        # interleave the marks and text lines
        text = [text_line + '\n' + marks_line for text_line, marks_line in zip(text, marks)]
    print('\n'.join(text))

def generate_chardiffs(textA, textB):
    marksA = [True for _ in range(len(textA))]              # initially assume all characters are marked
    marksB = [True for _ in range(len(textB))]

    _matcher.set_seqs(textA, textB)
    for block in _matcher.get_matching_blocks():
        # characters in the matching block are correct and should not be marked
        marksA[block.a : block.a + block.size] = [False for _ in range(block.size)]
        marksB[block.b : block.b + block.size] = [False for _ in range(block.size)]

    return (marksA, marksB)

TestPair = dataclasses.make_dataclass('TestPair', ('true', 'given'))

class BlackboxTest:

    def __init__(self, id_, stdin, true_stdout, true_stderr):
        self.id_ = id_
        self.stdin = stdin
        self.stdout = TestPair(true_stdout, None)
        self.stderr = TestPair(true_stderr, None)

    def run(self, exec_file):
        run_obj = subprocess.run([exec_file, '-nodoc'], capture_output=True, input=self.stdin, text=True)
        self.stdout.given = run_obj.stdout
        self.stderr.given = run_obj.stderr

    @classmethod
    def load(cls, test_file):
        with Path(test_file).open() as f:
            contents = yaml.safe_load(f)
            errors, tests = contents['errors'], contents['tests']
            for test in tests:
                if 'stderr' in test:
                    for error, msg in errors.items():
                        test['stderr'] = test['stderr'].replace('$' + error, msg)
                else:
                    test['stderr'] = ''                     # stderr if omitted is assumed to be blank
            tests = [
                cls(id_=idx+1, stdin=test['stdin'], true_stdout=test['stdout'], true_stderr=test['stderr'])
                for idx, test in enumerate(tests)]
            return tests

    def print(self, *args, **kwargs):
        print(f'Case {self.id_}')
        print_chardiff(self.stdin, *args, marks=None, label='stdin', **kwargs)
        for attrname in ('stdout', 'stderr'):
            attr = getattr(self, attrname)
            if attr.true != attr.given:
                true_marks, given_marks = generate_chardiffs(attr.true, attr.given)
                print_chardiff(attr.true, *args, marks=true_marks, label=attrname + '(true)', **kwargs)
                print_chardiff(attr.given, *args, marks=given_marks, label=attrname + '(given)', **kwargs)
        print('---')

    @property
    def passed(self):
        return (self.stdout.true == self.stdout.given) and (self.stderr.true == self.stderr.given)

count = 0
tests = BlackboxTest.load('tests/testcases.yml')
for test in tests:
    test.run(Path.cwd().joinpath('a.out'))
    test.print(textwidth=90, labelwidth=20)
    if test.passed: count += 1

print(f'{count}/{len(tests)} passed.')
sys.exit(0 if count == len(tests) else 2);   # exit with code 2 if some tests fail
