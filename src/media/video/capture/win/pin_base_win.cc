// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/pin_base_win.h"

#include "base/logging.h"

namespace media {

// Implement IEnumPins.
class TypeEnumerator
    : public IEnumMediaTypes,
      public base::RefCounted<TypeEnumerator> {
 public:
  explicit TypeEnumerator(PinBase* pin)
      : pin_(pin),
        index_(0) {
  }

  ~TypeEnumerator() {
  }

  // Implement from IUnknown.
  STDMETHOD(QueryInterface)(REFIID iid, void** object_ptr) {
    if (iid == IID_IEnumMediaTypes || iid == IID_IUnknown) {
      AddRef();
      *object_ptr = static_cast<IEnumMediaTypes*>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHOD_(ULONG, AddRef)() {
    base::RefCounted<TypeEnumerator>::AddRef();
    return 1;
  }

  STDMETHOD_(ULONG, Release)() {
    base::RefCounted<TypeEnumerator>::Release();
    return 1;
  }

  // Implement IEnumMediaTypes.
  STDMETHOD(Next)(ULONG count, AM_MEDIA_TYPE** types, ULONG* fetched) {
    ULONG types_fetched = 0;

    while (types_fetched < count) {
      // Allocate AM_MEDIA_TYPE that we will store the media type in.
      AM_MEDIA_TYPE* type = reinterpret_cast<AM_MEDIA_TYPE*>(CoTaskMemAlloc(
          sizeof(AM_MEDIA_TYPE)));
      if (!type) {
        FreeAllocatedMediaTypes(types_fetched, types);
        return E_OUTOFMEMORY;
      }
      ZeroMemory(type, sizeof(AM_MEDIA_TYPE));

      // Allocate a VIDEOINFOHEADER and connect it to the AM_MEDIA_TYPE.
      type->cbFormat = sizeof(VIDEOINFOHEADER);
      BYTE *format = reinterpret_cast<BYTE*>(CoTaskMemAlloc(
          sizeof(VIDEOINFOHEADER)));
      if (!format) {
        CoTaskMemFree(type);
        FreeAllocatedMediaTypes(types_fetched, types);
        return E_OUTOFMEMORY;
      }
      type->pbFormat = format;
      // Get the media type from the pin.
      if (pin_->GetValidMediaType(index_++, type)) {
        types[types_fetched++] = type;
      } else {
        CoTaskMemFree(format);
        CoTaskMemFree(type);
        break;
      }
    }

    if (fetched)
      *fetched = types_fetched;

    return types_fetched == count ? S_OK : S_FALSE;
  }

  STDMETHOD(Skip)(ULONG count) {
    index_ += count;
    return S_OK;
  }

  STDMETHOD(Reset)() {
    index_ = 0;
    return S_OK;
  }

  STDMETHOD(Clone)(IEnumMediaTypes** clone) {
    TypeEnumerator* type_enum = new TypeEnumerator(pin_);
    if (!type_enum)
      return E_OUTOFMEMORY;
    type_enum->AddRef();
    type_enum->index_ = index_;
    *clone = type_enum;
    return S_OK;
  }

 private:
  void FreeAllocatedMediaTypes(ULONG allocated, AM_MEDIA_TYPE** types) {
    for (ULONG i = 0; i < allocated; ++i) {
      CoTaskMemFree(types[i]->pbFormat);
      CoTaskMemFree(types[i]);
    }
  }

  scoped_refptr<PinBase> pin_;
  int index_;
};

PinBase::PinBase(IBaseFilter* owner)
    : owner_(owner) {
}

PinBase::~PinBase() {
}

void PinBase::SetOwner(IBaseFilter* owner) {
  owner_ = owner;
}

// Called on an output pin to and establish a
//   connection.
STDMETHODIMP PinBase::Connect(IPin* receive_pin,
                              const AM_MEDIA_TYPE* media_type) {
  if (!receive_pin || !media_type)
    return E_POINTER;

  current_media_type_ = *media_type;
  receive_pin->AddRef();
  connected_pin_.Attach(receive_pin);
  HRESULT hr = receive_pin->ReceiveConnection(this, media_type);

  return hr;
}

// Called from an output pin on an input pin to and establish a
// connection.
STDMETHODIMP PinBase::ReceiveConnection(IPin* connector,
                                        const AM_MEDIA_TYPE* media_type) {
  if (!IsMediaTypeValid(media_type))
    return VFW_E_TYPE_NOT_ACCEPTED;

  current_media_type_ = current_media_type_;
  connector->AddRef();
  connected_pin_.Attach(connector);
  return S_OK;
}

STDMETHODIMP PinBase::Disconnect() {
  if (!connected_pin_)
    return S_FALSE;

  connected_pin_.Release();
  return S_OK;
}

STDMETHODIMP PinBase::ConnectedTo(IPin** pin) {
  *pin = connected_pin_;
  if (!connected_pin_)
    return VFW_E_NOT_CONNECTED;

  connected_pin_.get()->AddRef();
  return S_OK;
}

STDMETHODIMP PinBase::ConnectionMediaType(AM_MEDIA_TYPE* media_type) {
  if (!connected_pin_)
    return VFW_E_NOT_CONNECTED;
  *media_type = current_media_type_;
  return S_OK;
}

STDMETHODIMP PinBase::QueryPinInfo(PIN_INFO* info) {
  info->dir = PINDIR_INPUT;
  info->pFilter = owner_;
  if (owner_)
    owner_->AddRef();
  info->achName[0] = L'\0';

  return S_OK;
}

STDMETHODIMP PinBase::QueryDirection(PIN_DIRECTION* pin_dir) {
  *pin_dir = PINDIR_INPUT;
  return S_OK;
}

STDMETHODIMP PinBase::QueryId(LPWSTR* id) {
  NOTREACHED();
  return E_OUTOFMEMORY;
}

STDMETHODIMP PinBase::QueryAccept(const AM_MEDIA_TYPE* media_type) {
  return S_FALSE;
}

STDMETHODIMP PinBase::EnumMediaTypes(IEnumMediaTypes** types) {
  *types = new TypeEnumerator(this);
  (*types)->AddRef();
  return S_OK;
}

STDMETHODIMP PinBase::QueryInternalConnections(IPin** pins, ULONG* no_pins) {
  return E_NOTIMPL;
}

STDMETHODIMP PinBase::EndOfStream() {
  return S_OK;
}

STDMETHODIMP PinBase::BeginFlush() {
  return S_OK;
}

STDMETHODIMP PinBase::EndFlush() {
  return S_OK;
}

STDMETHODIMP PinBase::NewSegment(REFERENCE_TIME start,
                                 REFERENCE_TIME stop,
                                 double rate) {
  NOTREACHED();
  return E_NOTIMPL;
}

// Inherited from IMemInputPin.
STDMETHODIMP PinBase::GetAllocator(IMemAllocator** allocator) {
  return VFW_E_NO_ALLOCATOR;
}

STDMETHODIMP PinBase::NotifyAllocator(IMemAllocator* allocator,
                                      BOOL read_only) {
  return S_OK;
}

STDMETHODIMP PinBase::GetAllocatorRequirements(
    ALLOCATOR_PROPERTIES* properties) {
  return E_NOTIMPL;
}

STDMETHODIMP PinBase::ReceiveMultiple(IMediaSample** samples,
                                      long sample_count,
                                      long* processed) {
  NOTREACHED();
  return VFW_E_INVALIDMEDIATYPE;
}

STDMETHODIMP PinBase::ReceiveCanBlock() {
  return S_FALSE;
}

// Inherited from IUnknown.
STDMETHODIMP PinBase::QueryInterface(REFIID id, void** object_ptr) {
  if (id == IID_IPin || id == IID_IUnknown) {
    *object_ptr = static_cast<IPin*>(this);
  } else if (id == IID_IMemInputPin) {
    *object_ptr = static_cast<IMemInputPin*>(this);
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

STDMETHODIMP_(ULONG) PinBase::AddRef() {
  base::RefCounted<PinBase>::AddRef();
  return 1;
}

STDMETHODIMP_(ULONG) PinBase::Release() {
  base::RefCounted<PinBase>::Release();
  return 1;
}

}  // namespace media
