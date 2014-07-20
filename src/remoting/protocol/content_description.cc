// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/content_description.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "remoting/base/constants.h"
#include "remoting/proto/auth.pb.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::QName;
using buzz::XmlElement;

namespace remoting {
namespace protocol {

const char ContentDescription::kChromotingContentName[] = "chromoting";

namespace {

const char kDefaultNs[] = "";

// Following constants are used to format session description in XML.
const char kDescriptionTag[] = "description";
const char kControlTag[] = "control";
const char kEventTag[] = "event";
const char kVideoTag[] = "video";
const char kResolutionTag[] = "initial-resolution";
const char kAuthenticationTag[] = "authentication";
const char kCertificateTag[] = "certificate";
const char kAuthTokenTag[] = "auth-token";

const char kTransportAttr[] = "transport";
const char kVersionAttr[] = "version";
const char kCodecAttr[] = "codec";
const char kWidthAttr[] = "width";
const char kHeightAttr[] = "height";

const char kStreamTransport[] = "stream";
const char kDatagramTransport[] = "datagram";
const char kSrtpTransport[] = "srtp";
const char kRtpDtlsTransport[] = "rtp-dtls";

const char kVp8Codec[] = "vp8";
const char kZipCodec[] = "zip";

const char* GetTransportName(ChannelConfig::TransportType type) {
  switch (type) {
    case ChannelConfig::TRANSPORT_STREAM:
      return kStreamTransport;
    case ChannelConfig::TRANSPORT_DATAGRAM:
      return kDatagramTransport;
    case ChannelConfig::TRANSPORT_SRTP:
      return kSrtpTransport;
    case ChannelConfig::TRANSPORT_RTP_DTLS:
      return kRtpDtlsTransport;
  }
  NOTREACHED();
  return NULL;
}

const char* GetCodecName(ChannelConfig::Codec type) {
  switch (type) {
    case ChannelConfig::CODEC_VP8:
      return kVp8Codec;
    case ChannelConfig::CODEC_ZIP:
      return kZipCodec;
    default:
      break;
  }
  NOTREACHED();
  return NULL;
}


// Format a channel configuration tag for chromotocol session description,
// e.g. for video channel:
//    <video transport="srtp" version="1" codec="vp8" />
XmlElement* FormatChannelConfig(const ChannelConfig& config,
                                const std::string& tag_name) {
  XmlElement* result = new XmlElement(
      QName(kChromotingXmlNamespace, tag_name));

  result->AddAttr(QName(kDefaultNs, kTransportAttr),
                   GetTransportName(config.transport));

  result->AddAttr(QName(kDefaultNs, kVersionAttr),
                  base::IntToString(config.version));

  if (config.codec != ChannelConfig::CODEC_UNDEFINED) {
    result->AddAttr(QName(kDefaultNs, kCodecAttr),
                    GetCodecName(config.codec));
  }

  return result;
}

bool ParseTransportName(const std::string& value,
                        ChannelConfig::TransportType* transport) {
  if (value == kStreamTransport) {
    *transport = ChannelConfig::TRANSPORT_STREAM;
  } else if (value == kDatagramTransport) {
    *transport = ChannelConfig::TRANSPORT_DATAGRAM;
  } else if (value == kSrtpTransport) {
    *transport = ChannelConfig::TRANSPORT_SRTP;
  } else if (value == kRtpDtlsTransport) {
    *transport = ChannelConfig::TRANSPORT_RTP_DTLS;
  } else {
    return false;
  }
  return true;
}

bool ParseCodecName(const std::string& value, ChannelConfig::Codec* codec) {
  if (value == kVp8Codec) {
    *codec = ChannelConfig::CODEC_VP8;
  } else if (value == kZipCodec) {
    *codec = ChannelConfig::CODEC_ZIP;
  } else {
    return false;
  }
  return true;
}

// Returns false if the element is invalid.
bool ParseChannelConfig(const XmlElement* element, bool codec_required,
                        ChannelConfig* config) {
  if (!ParseTransportName(element->Attr(QName(kDefaultNs, kTransportAttr)),
                          &config->transport) ||
      !base::StringToInt(element->Attr(QName(kDefaultNs, kVersionAttr)),
                         &config->version)) {
    return false;
  }

  if (codec_required) {
    if (!ParseCodecName(element->Attr(QName(kDefaultNs, kCodecAttr)),
                        &config->codec)) {
      return false;
    }
  } else {
    config->codec = ChannelConfig::CODEC_UNDEFINED;
  }

  return true;
}

}  // namespace

ContentDescription::ContentDescription(
    const CandidateSessionConfig* candidate_config,
    const std::string& auth_token,
    const std::string& certificate)
    : candidate_config_(candidate_config),
      auth_token_(auth_token),
      certificate_(certificate) {
}

ContentDescription::~ContentDescription() { }

// ToXml() creates content description for chromoting session. The
// description looks as follows:
//   <description xmlns="google:remoting">
//     <control transport="stream" version="1" />
//     <event transport="datagram" version="1" />
//     <video transport="srtp" codec="vp8" version="1" />
//     <initial-resolution width="800" height="600" />
//     <authentication>
//       <certificate>[BASE64 Encoded Certificate]</certificate>
//       <auth-token>...</auth-token>  // IT2Me only.
//     </authentication>
//   </description>
//
XmlElement* ContentDescription::ToXml() const {
  XmlElement* root = new XmlElement(
      QName(kChromotingXmlNamespace, kDescriptionTag), true);

  std::vector<ChannelConfig>::const_iterator it;

  for (it = config()->control_configs().begin();
       it != config()->control_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kControlTag));
  }

  for (it = config()->event_configs().begin();
       it != config()->event_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kEventTag));
  }

  for (it = config()->video_configs().begin();
       it != config()->video_configs().end(); ++it) {
    root->AddElement(FormatChannelConfig(*it, kVideoTag));
  }

  XmlElement* resolution_tag = new XmlElement(
      QName(kChromotingXmlNamespace, kResolutionTag));
  resolution_tag->AddAttr(QName(kDefaultNs, kWidthAttr),
                          base::IntToString(
                              config()->initial_resolution().width));
  resolution_tag->AddAttr(QName(kDefaultNs, kHeightAttr),
                          base::IntToString(
                              config()->initial_resolution().height));
  root->AddElement(resolution_tag);

  if (!certificate().empty() || !auth_token().empty()) {
    XmlElement* authentication_tag = new XmlElement(
        QName(kChromotingXmlNamespace, kAuthenticationTag));

    if (!certificate().empty()) {
      XmlElement* certificate_tag = new XmlElement(
          QName(kChromotingXmlNamespace, kCertificateTag));

      std::string base64_cert;
      if (!base::Base64Encode(certificate(), &base64_cert)) {
        LOG(DFATAL) << "Cannot perform base64 encode on certificate";
      }

      certificate_tag->SetBodyText(base64_cert);
      authentication_tag->AddElement(certificate_tag);
    }

    if (!auth_token().empty()) {
      XmlElement* auth_token_tag = new XmlElement(
          QName(kChromotingXmlNamespace, kAuthTokenTag));
      auth_token_tag->SetBodyText(auth_token());
      authentication_tag->AddElement(auth_token_tag);
    }

    root->AddElement(authentication_tag);
  }

  return root;
}

// static
ContentDescription* ContentDescription::ParseXml(
    const XmlElement* element) {
  if (element->Name() == QName(kChromotingXmlNamespace, kDescriptionTag)) {
    scoped_ptr<CandidateSessionConfig> config(
        CandidateSessionConfig::CreateEmpty());
    const XmlElement* child = NULL;

    // <control> tags.
    QName control_tag(kChromotingXmlNamespace, kControlTag);
    child = element->FirstNamed(control_tag);
    while (child) {
      ChannelConfig channel_config;
      if (!ParseChannelConfig(child, false, &channel_config))
        return NULL;
      config->mutable_control_configs()->push_back(channel_config);
      child = child->NextNamed(control_tag);
    }

    // <event> tags.
    QName event_tag(kChromotingXmlNamespace, kEventTag);
    child = element->FirstNamed(event_tag);
    while (child) {
      ChannelConfig channel_config;
      if (!ParseChannelConfig(child, false, &channel_config))
        return NULL;
      config->mutable_event_configs()->push_back(channel_config);
      child = child->NextNamed(event_tag);
    }

    // <video> tags.
    QName video_tag(kChromotingXmlNamespace, kVideoTag);
    child = element->FirstNamed(video_tag);
    while (child) {
      ChannelConfig channel_config;
      if (!ParseChannelConfig(child, true, &channel_config))
        return NULL;
      config->mutable_video_configs()->push_back(channel_config);
      child = child->NextNamed(video_tag);
    }

    // <initial-resolution> tag.
    child = element->FirstNamed(QName(kChromotingXmlNamespace, kResolutionTag));
    if (!child)
      return NULL; // Resolution must always be specified.
    int width;
    int height;
    if (!base::StringToInt(child->Attr(QName(kDefaultNs, kWidthAttr)),
                           &width) ||
        !base::StringToInt(child->Attr(QName(kDefaultNs, kHeightAttr)),
                           &height)) {
      return NULL;
    }
    ScreenResolution resolution(width, height);
    if (!resolution.IsValid()) {
      return NULL;
    }

    *config->mutable_initial_resolution() = resolution;

    // Parse authentication information.
    std::string certificate;
    std::string auth_token;
    child = element->FirstNamed(QName(kChromotingXmlNamespace,
                                      kAuthenticationTag));
    if (child) {
      // Parse the certificate.
      const XmlElement* cert_tag =
          child->FirstNamed(QName(kChromotingXmlNamespace, kCertificateTag));
      if (cert_tag) {
        std::string base64_cert = cert_tag->BodyText();
        if (!base::Base64Decode(base64_cert, &certificate)) {
          LOG(ERROR) << "Failed to decode certificate received from the peer.";
          return NULL;
        }
      }

      // Parse auth-token.
      const XmlElement* auth_token_tag =
          child->FirstNamed(QName(kChromotingXmlNamespace, kAuthTokenTag));
      if (auth_token_tag) {
        auth_token = auth_token_tag->BodyText();
      }
    }

    return new ContentDescription(config.release(), auth_token, certificate);
  }
  LOG(ERROR) << "Invalid description: " << element->Str();
  return NULL;
}

}  // namespace protocol
}  // namespace remoting
