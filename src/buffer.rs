use std::slice;
use std::vec::Vec;
use std::ops::Deref;
use std::marker::PhantomData;

pub struct Buffer<'a> {
    data: *const u8,
    length: usize,
    destructor: Option<fn(*const u8)>,
    next: Option<Box<Buffer<'a>>>,
    __: PhantomData<&'a [u8]>
}

impl<'a> Buffer<'a> {
    pub unsafe fn new_direct(data: *const u8, length: usize, destructor: Option<fn(*const u8)>) -> Buffer<'a>
    {
        Buffer {
            data: data,
            length: length,
            destructor: destructor,
            next: None,
            __: PhantomData
        }
    }

    pub fn new(data: &'a [u8]) -> Buffer<'a>
    {
        unsafe { Buffer::new_direct(&data[0], data.len(), None) }
    }

    pub fn len(&self) -> usize
    {
        let mut size = self.length;
        if let Some(ref next) = self.next {
            size += next.len();
        }
        size
    }
    
    pub fn append<T: Into<Buffer<'a>>>(&mut self, buf: T)
    {
        match self.next {
            Some(ref mut next) => next.append(buf.into()),
            None => self.next = Some(Box::new(buf.into()))
        }
    }

    fn to_slice_partial(&self) -> &'a [u8]
    {
        unsafe { slice::from_raw_parts(self.data, self.length) }
    }

    pub fn to_vec(&self) -> Vec<u8>
    {
        let mut vec = self.to_slice_partial().to_vec();
        match self.next {
            None => vec,
            Some(ref next) => {
                vec.append(&mut next.to_vec());
                vec
            }
        }
    }
}

impl<'a> From<&'a[u8]> for Buffer<'a> {
    fn from(data: &'a[u8]) -> Buffer<'a>
    {
        Buffer::new(data)
    }
}

impl<'a> Drop for Buffer<'a> {
    fn drop(&mut self)
    {
        if let Some(ref d) = self.destructor {
            (d)(self.data);
        }
    }
}



#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn new_len()
    {
        let buf = Buffer::new(b"hello");
        assert_eq!(5, buf.len());
        assert_eq!(b"hello".to_vec(), buf.to_vec());
    }

    #[test]
    fn append_len()
    {
        let mut buf = Buffer::new(b"abc");
        buf.append(b"def" as &[u8]);
        assert_eq!(6, buf.len());
        assert_eq!(b"abcdef".to_vec(), buf.to_vec());
    }
}