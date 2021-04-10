(defun (print-memory)
    (print "Memory usage: " (kilobytes (memtotal)) "KB"))
(print-memory)
;; Allocate a million number objects
(defun (call-this n)
    (if (= n 0)
        0
        (call-this (- n 1))))
(call-this (* 128 16))
(print-memory)
