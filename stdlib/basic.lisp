(defun (case value . cases)
    (if (= cases '())
        nil
        (if (= value nil)
            nil
            (if (= value (car (car cases)))
                (cadr (car cases))
              (case value . (cdr cases))))))

(defun (kilobytes nb)
    (/ nb 1000))

(defun (megabytes nb)
    (/ nb 1000000))

(defun (gigabytes nb)
    (/ nb 1000000000))

(defun (append list1 list2)
    (if (null? list1)
        list2
        (cons (car list1) (append (cdr list1) list2))))

(defun (length items)
    (defun (length-iter a count)
        (if (null? a)
            count
          (length-iter (cdr a) (+ 1 count))))
     (length-iter items 0))

(defun (reverse l)
    (if (null? l)
        '()
        (cons (reverse (cdr l)) (car l))))

(defun (map p l)
    (if (null? l)
        '()
        (cons (p (car l)) (map p (cdr l)))))

(defun (for-each p l)
    (if (null? l)
       nil
       (begin
         (p (car l))
         (for-each p (cdr l)))))

(defun (accumulate p l b)
    (if (null? l)
        b
        (p (accumulate p (cdr l) b) (car l))))
