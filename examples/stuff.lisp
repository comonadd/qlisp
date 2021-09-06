(defun (divides? a b) (= (remainder b a) 0))

(defun (square a) (* a a))

(defun (find-divisor n test-divisor)
  (cond ((> (square test-divisor) n) n)
        ((divides? test-divisor n) test-divisor)
        (else (find-divisor n (+ test-divisor 1)))))

(defun (smallest-divisor n) (find-divisor n 2))

(defun (prime? n)
  (= n (smallest-divisor n)))

(print "1 prime: " (prime? 1))
(print "2 prime: " (prime? 2))
(print "3 prime: " (prime? 3))
(print "8 prime: " (prime? 8))
(print "589 prime: " (prime? 589))
