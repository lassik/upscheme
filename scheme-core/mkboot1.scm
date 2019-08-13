; -*- scheme -*-

(load "system.scm")
(load "compiler.scm")
(load "dump.scm")

(dump-buffer-as-c-literal (system-image->buffer))
