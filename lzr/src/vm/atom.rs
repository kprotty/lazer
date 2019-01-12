use super::os::{*, ffi::*};
use super::{Hash, Hashable};

#[repr(C)]
pub struct Atom {
    size_and_displacement: u32,
    hash: Hash,
    data: usize,
}

impl Hashable for Atom {
    fn hashed(&self) -> Hash {
        self.hash
    }
}

impl Atom { 
    pub const MAX_TEXT_SIZE: usize = core::u8::MAX as usize;
    pub const MAX_ATOM_SIZE: usize = size_of::<Atom>() + Atom::MAX_TEXT_SIZE;

    #[inline]
    pub fn as_bytes(&self) -> &[u8] {
        unsafe { core::slice::from_raw_parts(self.as_ptr() as *const _, self.len()) }
    }

    #[inline]
    pub fn as_str(&self) -> &str {
        unsafe { core::str::from_utf8_unchecked(self.as_bytes()) }
    }

    #[inline]
    pub fn as_ptr(&self) -> *mut u8 {
        unsafe { (self as *const Self).offset(1) as *mut u8 }
    }

    #[inline]
    pub fn len(&self) -> usize {
        (self.size_and_displacement & 0xff) as usize
    }

    #[inline]
    pub fn displacement(&self) -> usize {
        (self.size_and_displacement >> 8) as usize
    }

    #[inline]
    pub fn bump_displacement(&mut self) {
        let displacement = self.displacement() as u32;
        self.size_and_displacement |= (displacement + 1) << 8;
    }
}

pub struct AtomTable {
    memory: AtomMemory,
    mapping: AtomMapping,
}

impl AtomTable {
    pub fn new(max_atoms: usize) -> Option<Self> {
        try {
            Self {
                memory: AtomMemory::new(max_atoms)?,
                mapping: AtomMapping::new(max_atoms)?,
            }
        }
    }

    #[inline]
    pub fn find(&self, text: &[u8]) -> Option<*mut Atom> {
        if text.len() <= Atom::MAX_TEXT_SIZE {
            self.mapping.find(text)
        } else {
            None
        }
    }

    pub fn emplace(&mut self, text: &[u8]) -> Option<*mut Atom> {
        if text.len() <= Atom::MAX_TEXT_SIZE {
            self.mapping.find(text).or_else(|| {
                self.memory.alloc(text).and_then(|atom| {
                    self.mapping.insert(atom)
                })
            })
        } else {
            None
        }
    }
}

struct AtomMemory {
    top: usize,
    memory: Page<u8>,
    committed: usize,
}

impl AtomMemory {
    const FLAGS: usize = PAGE_READ | PAGE_WRITE;

    pub fn new(max_atoms: usize) -> Option<Self> {
        let memory = max_atoms * Atom::MAX_ATOM_SIZE;
        Page::mmap(memory, Self::FLAGS).map(|page| Self {
            top: 0,
            memory: page,
            committed: 0,
        })
    }

    pub fn alloc(&mut self, text: &[u8]) -> Option<*mut Atom> {
        self.alloc_atom(size_of::<Atom>() + text.len()).map(|atom| unsafe {
            memcpy((*atom).as_ptr(), text.as_ptr(), text.len());
            (*atom).size_and_displacement = text.len() as _;
            (*atom).hash = text.hashed();
            (*atom).data = 0;
            atom
        })
    }

    #[inline]
    fn alloc_atom(&mut self, memory_size: usize) -> Option<*mut Atom> {
        if self.top + memory_size <= self.memory.len() {
            let offset = self.top;
            self.top += memory_size;
            self.ensure_committed();
            Some(self.memory.offset(offset) as *mut Atom)
        } else {
            None
        }
    }

    #[inline]
    fn ensure_committed(&mut self) {
        if self.committed < self.top {
            let commit_size = align(self.top - self.committed, 4096);
            self.memory.advise(commit_size, Self::FLAGS | PAGE_COMMIT);
            self.committed += commit_size;
        }
    }
}

struct AtomMapping {
    size: usize,
    capacity_mask: usize,
    atoms: *mut *mut Atom,
    memory: Page<*mut Atom>,
}

impl core::ops::Drop for AtomMapping {
    fn drop(&mut self) {
        replace(&mut self.memory, Page::empty()).unmap();
    }
}

impl AtomMapping {
    const FLAGS: usize = PAGE_READ | PAGE_WRITE;

    pub fn new(max_atoms: usize) -> Option<Self> {
        let max_atoms = max_atoms.next_power_of_two().max(8);
        Page::mmap(max_atoms * 2, Self::FLAGS).map(|memory| {

            let capacity = max_atoms.min(1024);
            memory.advise(capacity, Self::FLAGS | PAGE_COMMIT);

            Self {
                capacity_mask: capacity - 1,
                atoms: memory.offset(0),
                memory: memory,
                size: 0,
            }
        })
    }

    #[inline]
    fn matches(text: &[u8], text_hash: Hash, atom: &Atom) -> bool {
        text_hash == atom.hashed() &&
        text.len() == atom.len() &&
        text == atom.as_bytes()
    }

    pub fn find(&self, text: &[u8]) -> Option<*mut Atom> {
        let hash = text.hashed();
        let mut displacement = 0;
        let mut index = hash as usize & self.capacity_mask;

        loop {
            unsafe {
                let atom = self.atoms.offset(index as isize);
                if (*atom).is_null() || displacement > (**atom).displacement() {
                    return None
                } else if Self::matches(text, hash, &* (*atom)) {
                    return Some(*atom)
                }
            }

            displacement += 1;
            index = (index + 1) & self.capacity_mask;
        }
    }

    pub fn insert(&mut self, atom: *mut Atom) -> Option<*mut Atom> {
        if self.size >= self.capacity_mask + 1 && !self.grow_table() {
            None
        } else {
            unsafe { self.insert_atom(atom) }
        }
    }

    unsafe fn insert_atom(&mut self, atom: *mut Atom) -> Option<*mut Atom> {
        let mut current_atom = atom;
        (*atom).size_and_displacement &= 0xff;
        let mut index = (*atom).hashed() as usize & self.capacity_mask;

        loop {
            let atom_slot = self.atoms.offset(index as isize);
            if (*atom_slot).is_null() {
                self.size += 1;
                *atom_slot = current_atom;
                return Some(atom)
            } else if (**atom_slot).displacement() < (*current_atom).displacement() {
                swap(&mut *atom_slot, &mut current_atom);
            }

            index = (index + 1) & self.capacity_mask;
            (*current_atom).bump_displacement();
        }
    }

    fn grow_table(&mut self) -> bool {
        let old_capacity = self.capacity_mask + 1;
        let new_capacity = old_capacity << 1;
        let old_atoms = self.atoms;

        self.size = 0;
        self.capacity_mask = new_capacity - 1;
        self.atoms = if new_capacity * 2 > self.memory.len() {
            return false;
        } else if old_atoms == self.memory.offset(0) {
            self.memory.offset(self.memory.len() / 2)
        } else {
            self.memory.offset(0)
        };

        let old_page = Page::from((old_atoms, old_capacity));
        let new_page = Page::from((self.atoms, new_capacity));

        new_page.advise(new_capacity, Self::FLAGS | PAGE_COMMIT);
        old_page.advise(old_capacity, Self::FLAGS | PAGE_SEQUENTIAL);
        old_page
            .as_slice()
            .iter()
            .filter(|&atom| !(*atom).is_null())
            .for_each(|&atom| unsafe { self.insert_atom(atom); });

        old_page.advise(old_capacity, PAGE_DECOMMIT);
        true
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::collections::HashMap;

    #[test]
    fn test_atom_table() {
        const TEST_ATOMS: &'static [&'static [u8]] = &[
            b"atom", b"table", b"is", b"working",
            b"#this", b"!is", b"@nother", b"^est",
            b"012", b"345", b"678", b"90-",
            b"Kernel.Module.Example.Atom",
        ];

        let mut atom_table = AtomTable::new(5).unwrap();
        let mut map = HashMap::with_capacity(TEST_ATOMS.len());

        for test_atom in TEST_ATOMS {
            assert!(atom_table.find(test_atom).is_none());
            let atom_ptr = atom_table.emplace(test_atom).unwrap();
            assert!(!atom_ptr.is_null());
            map.insert(test_atom, atom_ptr);
        }

        for test_atom in TEST_ATOMS {
            let atom_ptr = atom_table.find(test_atom).unwrap();
            let real_ptr = map.get(test_atom).unwrap();
            assert_eq!(*real_ptr, atom_ptr);
        }
    }
}