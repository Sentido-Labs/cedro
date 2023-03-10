/** Check that comments and string/character literals are
 * parsed correctly.
 */
#pragma Cedro 1.0
const char* const hello = "hello \"world\\ no closing quote";
const char* const empty = "";
const char* const apos = '\'';
// Use the binary literals to detect whether this was swallowed as
// part of another token, which may happen if the end of a comment,
// string or character is not detected.
const x = 0b1101_0010;
const y = 0b1000_0000;
/**/
const z = 0b00_0010;
const t = 0b100_1010;
/**/
