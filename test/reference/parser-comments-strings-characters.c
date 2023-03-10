/** Check that comments and string/character literals are
 * parsed correctly.
 */
const char* const hello = "hello \"world\\ no closing quote";
const char* const empty = "";
const char* const apos = '\'';
// Use the binary literals to detect whether this was swallowed as
// part of another token, which may happen if the end of a comment,
// string or character is not detected.
const x = 0xD2;
const y = 0x80;
/**/
const z = 0x02;
const t = 0x4A;
/**/
