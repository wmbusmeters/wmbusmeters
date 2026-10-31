# The wmbusmeters security policy is NOTHING is secret!

Do not send private disclosures to me. Publish them directly!

If you use AI to find bugs or holes in wmbusmeters, spend some more
tokens to fix them directly and publish a PR instead. If it is a
significant hole and your PR solves it, then your name will
be registered in this file for eternal fame. A missed CHECK(y) that
permit buffer overflows would qualify here for example,
even if the buffer overflow cannot be exploited.

Tell your AI to read the AGENTS.md for how to write the text
describing the issue. I.e. I do not like long verbose texts.

I run fuzzing continuously on wmbusmeters (yes all the time) alas
this does not guarantee that there are no bugs, holes or other
security issues.

As an example of a serious security hole from 2021:
"Close a security hole where you could trivially spoof an encrypted meter
by sending an unencrypted telegram with the same id."
0224941e007c399eaac8f0bdf0eefbb0d8d6c974

Here is another crashing issue which was resolved:
https://github.com/wmbusmeters/wmbusmeters/issues/1788

Remember to use the words crash or segfault in the issue title
since this will get my attention immediately.
