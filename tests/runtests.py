import sys
import yaml
import subprocess
import unicodedata
from pathlib import Path
from collections import namedtuple
from difflib import SequenceMatcher

def _wrap(text, width, initial_indent='', subsequent_indent=''):
    assert len(initial_indent) < width
    assert len(subsequent_indent) < width

    lines = []
    while len(text) > 0:
        indent = initial_indent if len(lines) == 0 else subsequent_indent
        lines.append(indent + text[: width - len(indent)])
        text = text[width - len(indent): ]
    return lines

class CharacterDiff:
    """Presents character-wise diff markings for a single piece of text.

    If marks=None, then the text is displayed on its own without marks (not really a diff in this case, but just serves
    to wrap the text and optionally space it out).
    """

    def __init__(self, text, marks=None, label=''):
        self.text = text
        self.marks = marks
        self.label = label

        if self.marks is not None:
            # as many marks as there are characters in the text
            assert len(self.text) == len(self.marks)

            # due to the above assertion, no marks means no text.
            # we don't want to display the marks in such a case (it will be a blank line anyway).
            # we also write '<blank>' for the text so that it is clear that the text is blank (and the user is not
            # confused by the absence of any text).
            if len(self.marks) == 0:
                self.marks = None
                self.text = '<blank>'

    @staticmethod
    def _nspaces(n):
        return ''.join(' ' for _ in range(n))

    def display(self, textwidth, labelwidth=0, markchar='^', spacechar=unicodedata.lookup('BLACK CIRCLE'),
                spaceout=False):
        assert len(self.label) <= labelwidth
        marks = self.marks[:] if self.marks is not None else None
        text = self.text[:]

        if marks is not None:
            marks = [(markchar if mark else ' ') for mark in marks]
            marks = [mark + (' ' if char in ('\t', '\n', '\r') else '') for mark, char in zip(marks, text)]
            marks = (' ' if spaceout else '').join(marks)

        for old, new in ((' ', spacechar), ('\t', '\\t'), ('\n', '\\n'), ('\r', '\\r')):
            text = [new if char == old else char for char in text]
        text = (' ' if spaceout else '').join(text)

        # add the label as part of the text
        text = self.label + self._nspaces(labelwidth - len(self.label)) + text
        textwidth += labelwidth

        text = _wrap(text, textwidth, initial_indent=self._nspaces(0), subsequent_indent=self._nspaces(labelwidth))
        if marks is not None:
            marks = _wrap(marks, textwidth, initial_indent=self._nspaces(labelwidth),
                          subsequent_indent=self._nspaces(labelwidth))
            assert len(marks) == len(text)      # as many lines of marks as lines of text
            # interleave the marks and text lines
            text = [text_line + '\n' + marks_line for text_line, marks_line in zip(text, marks)]

        return '\n'.join(text)

class CharacterDiffPair:
    """Generates character-wise diff between two pieces of text.

    Use CharacterDiff to display the diffs corresponding to both of them.
    """

    _matcher = SequenceMatcher()

    def __init__(self, textA, textB, labelA='', labelB=''):
        # initially assume all characters are marked
        marksA = [True for _ in range(len(textA))]
        marksB = [True for _ in range(len(textB))]

        self._matcher.set_seqs(textA, textB)
        for block in self._matcher.get_matching_blocks():
            # characters in the matching block are correct and should not be marked
            marksA[block.a : block.a + block.size] = [False for _ in range(block.size)]
            marksB[block.b : block.b + block.size] = [False for _ in range(block.size)]

        self.diffA = CharacterDiff(textA, marksA, labelA)
        self.diffB = CharacterDiff(textB, marksB, labelB)

    def display(self, *args, **kwargs):
        return '\n'.join([self.diffA.display(*args, **kwargs), self.diffB.display(*args, **kwargs)])

class Testcase:
    """Representation of a test case.

    Holds the input to the translate program (stdin), the true or expected output (true_stdout), the true or expected
    error (true_stderr), the output given from the translate program (given_stdout), the error given from the translate
    program (given_stderr).
    """

    def __init__(self, stdin, true_stdout, true_stderr=''):
        self.stdin = stdin
        self.true_stdout = true_stdout
        self.true_stderr = true_stderr
        self.given_stdout = None
        self.given_stderr = None

    @property
    def passed(self):
        return self.true_stdout == self.given_stdout and self.true_stderr == self.given_stderr

    def run(self):
        run_obj = subprocess.run(
            [Path(__file__).parent.parent.joinpath('a.out'), '-nodoc'], input=self.stdin, text=True,
            capture_output=True)
        self.given_stdout = run_obj.stdout
        self.given_stderr = run_obj.stderr

    def display(self, *args, **kwargs):
        lines = []
        lines.append(CharacterDiff(self.stdin, label='stdin').display(*args, **kwargs))
        if self.true_stdout != self.given_stdout:
            lines.append(CharacterDiffPair(
                self.true_stdout, self.given_stdout, 'stdout (true)', 'stdout (given)').display(*args, **kwargs))
        if self.true_stderr != self.given_stderr:
            lines.append(CharacterDiffPair(
                self.true_stderr, self.given_stderr, 'stderr (true)', 'stderr (given)').display(*args, **kwargs))
        return '\n'.join(lines)

testfile = 'testrunner_testcases.yml' if debug else 'testcases.yml'
with Path(__file__).parent.joinpath(testfile).open() as f:
    testcases = yaml.safe_load_all(f)
    testcases = [case for doc in testcases for case in doc]     # doc = YAML document

count = 0
for idx, case in enumerate(testcases):
    case = Testcase(case['stdin'], true_stdout=case['stdout'], true_stderr=case.get('stderr', ''))
    case.run()
    print(f'Case #{idx + 1}')
    print(case.display(textwidth=90, labelwidth=20, spaceout=spaceout))
    print('---')
    count += (case.passed)

print(f'{count}/{idx+1} passed.')
sys.exit(0 if count == idx+1 else 2);   # exit with code 2 if some tests fail
