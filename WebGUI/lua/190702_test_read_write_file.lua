-- for safety consideration, you cannot read/write any file outside current running directory

filename = "hello.txt"

save_to_file(filename, "hello world!");
logln(get_from_file(filename))
