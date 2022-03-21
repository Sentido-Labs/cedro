/* Adapted from example in n2542.pdf#40 */

int f1(void) {
  puts("g called");
  if (bad1()) { return 1; }
  spin_lock(&lock1);
  if (bad2()) { spin_unlock(&lock1); return 1; }
  spin_lock(&lock2);
  if (bad()) { spin_unlock(&lock2); spin_unlock(&lock1); return 1; }

  /* Access data protected by the spinlock then force a panic */
  completed += 1;
  unforced(completed);

  spin_unlock(&lock2);
  spin_unlock(&lock1);

  return 0;
}
