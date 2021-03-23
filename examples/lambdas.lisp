(defun (hello callthis)
    (callthis 3 5))

(hello (lambda (a b) (print "Called it! a=" a ", b=" b)))
