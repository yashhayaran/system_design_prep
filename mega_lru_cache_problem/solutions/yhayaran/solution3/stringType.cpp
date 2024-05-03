/*
*   Description:            Class to match the basic features of std::string
*                           Strings represents the sequence of characters.
*   Features Supported:     1. String constuction/destruction.
*                           2. length function to count number of characters. 
*                           3. append function to
*                           4. + operator to join two strings.
*                           5. find function to find the set of characters or string (shorter) within 
*                              string.
*/
#include <iostream>
#include <exception>

class stringType
{
    public:
        stringType() : _pSeqOfChars(nullptr), _nSizeInBytes(0), _nLength(0)
        { }

        stringType(const char* pSeqOfChars) : _pSeqOfChars(nullptr), _nSizeInBytes(0), _nLength(0)
        { 
            _copyFromCharPointer(pSeqOfChars);
        }

        stringType(const stringType& stringObj)
        {
            _copyFromStringType(stringObj);
        }

        void operator= (const stringType& stringObj)
        {
            clear();
            _copyFromStringType(stringObj);
        }

        void operator= (const char* pSeqOfChars)
        {
            clear();
            _copyFromCharPointer(pSeqOfChars);
        }

        stringType operator+ (const stringType& strData)
        {
            stringType strResult;

            strResult.append(*this);
            strResult.append(strData);

            return  strResult;
        }

        virtual ~stringType()
        { 
            clear(); 
        }

        virtual void append(const char* pSeqOfChars)
        {
            if(pSeqOfChars != nullptr)
            {
                size_t nLenOfNewString = strlen(pSeqOfChars);
                if(nLenOfNewString > 0)
                {
                    size_t nNewSize = _nLength + nLenOfNewString + 1;
                    char* pBiggerString = new char[nNewSize];

                    if(_nLength > 0 && _pSeqOfChars != nullptr)
                    {
                        memcpy(pBiggerString, _pSeqOfChars, _nLength);
                        clear();
                    }

                    memcpy(pBiggerString + (nNewSize - nLenOfNewString - 1), pSeqOfChars, nLenOfNewString);
                    
                    // reset all
                    _pSeqOfChars = pBiggerString;
                    _nSizeInBytes = nNewSize;
                    _nLength = nNewSize - 1;

                    _pSeqOfChars[_nSizeInBytes] = '\0';
                }
            }
        }

        virtual void append(const stringType& strData)
        {
            if(strData._pSeqOfChars != nullptr && strData._nLength > 0)
            {
                    size_t nNewSize = _nLength + strData._nLength + 1;
                    char* pBiggerString = new char[nNewSize];

                    if(_nLength > 0 && _pSeqOfChars != nullptr)
                    {
                        memcpy(pBiggerString, _pSeqOfChars, _nLength);
                        clear();
                    }

                    memcpy(pBiggerString + (nNewSize - strData._nLength - 1), strData._pSeqOfChars, strData._nLength);
                    
                    // reset all
                    _pSeqOfChars = pBiggerString;
                    _nSizeInBytes = nNewSize;
                    _nLength = nNewSize - 1;

                    _pSeqOfChars[_nSizeInBytes] = '\0';
            }
        }

        void clear()
        {
            if(_nSizeInBytes > 0)
            {
                if(_pSeqOfChars != nullptr)
                { 
                    delete[] _pSeqOfChars;
                    _pSeqOfChars    = nullptr;
                    _nSizeInBytes   = 0;
                    _nLength        = 0;
                }
            }
        }

        size_t size()
        {
            return _nSizeInBytes;
        }

        size_t length()
        {
            return _nLength;
        }

        const char* c_str()
        {
            return _pSeqOfChars;
        }

    protected:
        char* _pSeqOfChars;
        size_t _nSizeInBytes;
        size_t _nLength;

        void _copyFromCharPointer(const char* pSeqOfChars)
        {
            if (pSeqOfChars != nullptr)
            {
                size_t nLength = strlen(pSeqOfChars);
                if(nLength > 0)
                {
                    _nLength = nLength;
                    _nSizeInBytes = _nLength + 1;

                    // new operator throws the std::bad_alloc excpetion in case of low memory
                    _pSeqOfChars = new char[_nSizeInBytes];
                    memcpy(_pSeqOfChars, pSeqOfChars, _nLength);
                    _pSeqOfChars[_nLength] = '\0';
                }
            }
        }

        void _copyFromStringType(const stringType& strData)
        {
            if (strData._nSizeInBytes > 0 && strData._pSeqOfChars != nullptr)
            {
                _nLength = strData._nLength;
                _nSizeInBytes = _nLength + 1;

                // new operator throws the std::bad_alloc excpetion in case of low memory
                _pSeqOfChars = new char[_nSizeInBytes];
                memcpy(_pSeqOfChars, strData._pSeqOfChars, _nLength);
                _pSeqOfChars[_nLength] = '\0';
            }
        }
};