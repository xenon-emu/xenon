import cpp

from FunctionCall fc, Function f
where fc.getTarget() = f and
      f.getName().matches("%_s")
select fc, "Use of MSVC-specific function " + f.getName()