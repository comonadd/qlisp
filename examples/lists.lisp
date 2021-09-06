(setq list-desc '(3 2 1))
(print "First item (car) of (3 2 1) is " (car list-desc))
(print "Second item (cadr) of (3 2 1) is " (cadr list-desc))
(print "cdr of (3 2 1) is " (cdr list-desc))

(setq abc (append list-desc '(5 6 7 8)))
(print "Append: " abc)
(print "Appended length: " (length abc))
(print "Reversed: " (reverse abc))

(setq mapped (map (lambda (a) (* a a)) '(1 2 3 4 5)))
(defun (scale-list l k)
    (map (lambda (x) (* x k)) l))
(print "Mapped: " mapped)
(print "Scaled x10: " (scale-list mapped 10))

(for-each (lambda (x) (print "x: " x)) (reverse '(3 2 1)))

(print "Accumulated: " (accumulate (lambda (acc x) (+ acc x)) abc 0))
