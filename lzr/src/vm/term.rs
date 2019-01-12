

#[derive(Debug, Clone, PartialEq)]
pub enum TermType {
    Number,
    Map,
    Atom,
    Data,
    List,
    Tuple,
    Binary,
    Function,
}

pub union Term {
    uint: u64,
    float: f64,
}

impl From<f64> for Term {
    fn from(number: f64) -> Self {
        Self { float: number }
    }
}

impl Term {
    pub fn from_ptr<T>(ptr: *mut T, term_type: TermType) -> Self {
        use self::TermType::*;
        let header = match term_type {
            Number      => 0,
            Map         => 1,
            Atom        => 2,
            Data        => 3,
            List        => 4,
            Tuple       => 5,
            Binary      => 6,
            Function    => 7,
        };
        Self { uint: ((header | 0xfff8) << 48) | (ptr as u64) }
    }

    pub fn get_type(&self) -> TermType {
        use self::TermType::*;
        unsafe {
            if self.uint <= (0xfff8u64 << 48) {
                Number
            } else {
                [Number, Map, Atom, Data, List, Tuple, Binary, Function]
                [((self.uint >> 48) & 0b111) as usize]
                .clone()
            }
        }
    }

    #[inline]
    pub fn as_ptr<T>(&self) -> *mut T {
        unsafe { (self.uint & 0xffffffffffff) as *mut T }
    }

    #[inline]
    pub fn as_ref<T>(&self) -> &mut T {
        unsafe { &mut *self.as_ptr() }
    }

    #[inline]
    pub fn as_number(&self) -> f64 {
        unsafe { self.float }
    }

    #[inline]
    pub fn is(&self, term_type: TermType) -> bool {
        self.get_type() == term_type
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_term_encoding() {
        const NUMBER: f64 = 3.141592;
        const TYPE: TermType = TermType::Data;
        const ADDR: *mut u8 = 0x1234 as *mut _;

        let term: Term = NUMBER.into();
        assert_eq!(term.as_number(), NUMBER);
        assert_eq!(term.get_type(), TermType::Number);

        let term: Term = Term::from_ptr(ADDR, TYPE);
        assert_eq!(term.as_ptr(), ADDR);
        assert_eq!(term.get_type(), TYPE);
    }
}