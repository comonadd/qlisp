;; Pass different numbers as arguments and just print them
(defun (variadic-fun . rest)
    (print rest))
(variadic-fun 3 1 2)

;; Pass two simple arguments and then also a few variadic
(defun (whatever a b c . rest)
    (print "a = " a ", b = " b ", c = " c ". Rest are: " rest))
(whatever 4 5 1 13 12 88)

;; Pass an expansion list and print it out
(whatever 4 5 1 . (cdr '(2 4)))
(variadic-fun . '(8 4 323))

;; TODO: Test invalid syntax also
