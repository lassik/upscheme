(load "system.scm")
(load "aliases.scm")
(load "compiler.scm")
(load "dump.scm")

(dump-buffers-as-c-literal (system-image->buffer))
