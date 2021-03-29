(defun (fib n)
    (case
        ((= n 0) 0)
        ((= n 1) 1)
        (else (+ (fib (- n 1)) (fib (- n 2))))))

(print "Fibonnaci of 3 is " (fib 3))
(print "Fibonnaci of 0 is " (fib 0))
(print "Fibonnaci of 1 is " (fib 1))
(print "Fibonnaci of 15 is " (fib 15))
