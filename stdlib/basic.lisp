(defun (case value . cases)
    (if (= cases '())
        nil
        (if (= value nil)
            nil
            (if (= value (car (car cases)))
                (cadr (car cases))
              (case value . (cdr cases))))))
