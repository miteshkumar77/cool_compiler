(*
 *  CS164 Fall 94
 *
 *  Programming Assignment 1
 *    Implementation of a simple stack machine.
 *
 *  Skeleton file
 *)

class StackNode inherits A2I {
  item: Object;
  next: StackNode;

  init(i: Object, n: StackNode): SELF_TYPE {
    {
      item <- i;
      next <- n;
      self;
    }
  };
  
  get_item(): Object { item };
  get_next(): StackNode { next };

  flatten(): String {
    let string: String <-
      case item of
        i: Int => i2a(i);
        s: String => s;
        o: Object => { abort(); ""; };
      esac
    in
      if (isvoid next) then
        string.concat("\n")
      else
        string.concat("\n").concat(next.flatten())
      fi
  };
};

class Stack inherits IO {
  head: StackNode;
  isEmpty(): Bool { isvoid head };
  push(item : Object): Object {
    {
      head <- (new StackNode).init(item, head);
      item;
    }
  };
  
  pop(): Object {
    if (isvoid head) then
      { 
        out_string("ERROR: pop called on empty stack\n");
        abort();
        head;
      }
    else
        let ret: StackNode <- head in
        {
          head <- head.get_next();
          ret.get_item();
        }
    fi
  };

  peek(): Object { head.get_item() };
  
  print(): Object { out_string(head.flatten()) };
};

class StackCmd inherits IO {
  ev(stk: Stack): Stack {
    {
      out_string("ERROR: StackCmd.ev() pure virtual method called\n");
      abort();
      stk;
    }
  };
};

class IntCmd inherits StackCmd {
  ev(stk: Stack): Stack { stk };
};

class PlusCmd inherits StackCmd {
  ev(stk: Stack): Stack {
    let
      ao: Object <- stk.pop(),
      bo: Object <- stk.pop(),
      a: Int <- 
        case ao of
          i: Int => i;
          o: Object => { out_string("stk.top() expected Int type\n"); abort(); 0; };
        esac,
      b: Int <-
        case bo of
          i: Int => i;
          o: Object => { out_string("stk.top() expected Int type\n"); abort(); 0; };
        esac
    in
      {
        stk.push(a+b);
        stk;
      }
  };
};

class SwpCmd inherits StackCmd {
  ev(stk: Stack): Stack {
    let
      a: Object <- stk.pop(),
      b: Object <- stk.pop()
    in
      {
        stk.push(a);
        stk.push(b);
        stk;
      }
  };
};

class EvCmd inherits StackCmd {
  ev(stk: Stack): Stack {
    if stk.isEmpty() then
      stk
    else
      case stk.peek() of
        i: Int => stk;
        s: String =>
            {
              stk.pop();
              if (s = "+") then
                (new PlusCmd).ev(stk)
              else
                (new SwpCmd).ev(stk)
              fi;
            };
        o: Object => { abort(); stk; };
      esac
    fi
  };
};

class Main inherits IO {

  main() : Object {
    let
      fin: Bool <- new Bool,
      stk: Stack <- new Stack
    in
      while (fin = false) loop
      {
        out_string(">");
        let
          inp: String <- in_string()
        in
          if (inp = "+") then
            stk.push(inp)
          else if (inp = "s") then
            stk.push(inp)
          else if (inp = "e") then
            (new EvCmd).ev(stk)  
          else if (inp = "d") then
            stk.print()
          else if (inp = "x") then
            fin <- true
          else
            stk.push((new A2I).a2i(inp))
          fi fi fi fi fi;
      }
      pool
  };

};