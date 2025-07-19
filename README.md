Currently only command-line program. Will be turned into a GUI/command-line program. Also I have ideas of how to rewrite it to be even faster.

Example:

`nowgrep -d C:/ -t Hello -f ".c, .h"`

- `-d/--directory`: The directory to search in
- `-t/--term`: The term to search for in files that pass filter
- `-f/--filter`: The file filter. Uses wildcards or exact file name match. One string, comma separated for multiple filters.
