class C {
	a : Int;
	b : Bool;
	init(x : Int, y : Bool) : C {
           {
		a <- x;
		b <- y;
		self;
           }
	};
};

class F inherits D {
	fn(x : Int) : String {
		""
	};
};

class D {
	
	fn(x : Int) : String {
		a
	};
	a : String <- "ABCD";
};

Class Main {
	main():C {
	 {
	  (new C).init(1,true);
	 }
	};
};
