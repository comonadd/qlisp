(defun (case . conds)
    (if (car (car conds))
        (cadr conds)
        (case (cdr conds))))
