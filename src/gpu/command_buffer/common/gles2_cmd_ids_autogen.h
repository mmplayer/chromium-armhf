// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is auto-generated from
// gpu/command_buffer/build_gles2_cmd_buffer.py
// DO NOT EDIT!

#ifndef GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_
#define GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_

#define GLES2_COMMAND_LIST(OP) \
  OP(ActiveTexture)                                            /* 256 */ \
  OP(AttachShader)                                             /* 257 */ \
  OP(BindAttribLocation)                                       /* 258 */ \
  OP(BindAttribLocationImmediate)                              /* 259 */ \
  OP(BindBuffer)                                               /* 260 */ \
  OP(BindFramebuffer)                                          /* 261 */ \
  OP(BindRenderbuffer)                                         /* 262 */ \
  OP(BindTexture)                                              /* 263 */ \
  OP(BlendColor)                                               /* 264 */ \
  OP(BlendEquation)                                            /* 265 */ \
  OP(BlendEquationSeparate)                                    /* 266 */ \
  OP(BlendFunc)                                                /* 267 */ \
  OP(BlendFuncSeparate)                                        /* 268 */ \
  OP(BufferData)                                               /* 269 */ \
  OP(BufferDataImmediate)                                      /* 270 */ \
  OP(BufferSubData)                                            /* 271 */ \
  OP(BufferSubDataImmediate)                                   /* 272 */ \
  OP(CheckFramebufferStatus)                                   /* 273 */ \
  OP(Clear)                                                    /* 274 */ \
  OP(ClearColor)                                               /* 275 */ \
  OP(ClearDepthf)                                              /* 276 */ \
  OP(ClearStencil)                                             /* 277 */ \
  OP(ColorMask)                                                /* 278 */ \
  OP(CompileShader)                                            /* 279 */ \
  OP(CompressedTexImage2D)                                     /* 280 */ \
  OP(CompressedTexImage2DImmediate)                            /* 281 */ \
  OP(CompressedTexSubImage2D)                                  /* 282 */ \
  OP(CompressedTexSubImage2DImmediate)                         /* 283 */ \
  OP(CopyTexImage2D)                                           /* 284 */ \
  OP(CopyTexSubImage2D)                                        /* 285 */ \
  OP(CreateProgram)                                            /* 286 */ \
  OP(CreateShader)                                             /* 287 */ \
  OP(CullFace)                                                 /* 288 */ \
  OP(DeleteBuffers)                                            /* 289 */ \
  OP(DeleteBuffersImmediate)                                   /* 290 */ \
  OP(DeleteFramebuffers)                                       /* 291 */ \
  OP(DeleteFramebuffersImmediate)                              /* 292 */ \
  OP(DeleteProgram)                                            /* 293 */ \
  OP(DeleteRenderbuffers)                                      /* 294 */ \
  OP(DeleteRenderbuffersImmediate)                             /* 295 */ \
  OP(DeleteShader)                                             /* 296 */ \
  OP(DeleteTextures)                                           /* 297 */ \
  OP(DeleteTexturesImmediate)                                  /* 298 */ \
  OP(DepthFunc)                                                /* 299 */ \
  OP(DepthMask)                                                /* 300 */ \
  OP(DepthRangef)                                              /* 301 */ \
  OP(DetachShader)                                             /* 302 */ \
  OP(Disable)                                                  /* 303 */ \
  OP(DisableVertexAttribArray)                                 /* 304 */ \
  OP(DrawArrays)                                               /* 305 */ \
  OP(DrawElements)                                             /* 306 */ \
  OP(Enable)                                                   /* 307 */ \
  OP(EnableVertexAttribArray)                                  /* 308 */ \
  OP(Finish)                                                   /* 309 */ \
  OP(Flush)                                                    /* 310 */ \
  OP(FramebufferRenderbuffer)                                  /* 311 */ \
  OP(FramebufferTexture2D)                                     /* 312 */ \
  OP(FrontFace)                                                /* 313 */ \
  OP(GenBuffers)                                               /* 314 */ \
  OP(GenBuffersImmediate)                                      /* 315 */ \
  OP(GenerateMipmap)                                           /* 316 */ \
  OP(GenFramebuffers)                                          /* 317 */ \
  OP(GenFramebuffersImmediate)                                 /* 318 */ \
  OP(GenRenderbuffers)                                         /* 319 */ \
  OP(GenRenderbuffersImmediate)                                /* 320 */ \
  OP(GenTextures)                                              /* 321 */ \
  OP(GenTexturesImmediate)                                     /* 322 */ \
  OP(GetActiveAttrib)                                          /* 323 */ \
  OP(GetActiveUniform)                                         /* 324 */ \
  OP(GetAttachedShaders)                                       /* 325 */ \
  OP(GetAttribLocation)                                        /* 326 */ \
  OP(GetAttribLocationImmediate)                               /* 327 */ \
  OP(GetBooleanv)                                              /* 328 */ \
  OP(GetBufferParameteriv)                                     /* 329 */ \
  OP(GetError)                                                 /* 330 */ \
  OP(GetFloatv)                                                /* 331 */ \
  OP(GetFramebufferAttachmentParameteriv)                      /* 332 */ \
  OP(GetIntegerv)                                              /* 333 */ \
  OP(GetProgramiv)                                             /* 334 */ \
  OP(GetProgramInfoLog)                                        /* 335 */ \
  OP(GetRenderbufferParameteriv)                               /* 336 */ \
  OP(GetShaderiv)                                              /* 337 */ \
  OP(GetShaderInfoLog)                                         /* 338 */ \
  OP(GetShaderPrecisionFormat)                                 /* 339 */ \
  OP(GetShaderSource)                                          /* 340 */ \
  OP(GetString)                                                /* 341 */ \
  OP(GetTexParameterfv)                                        /* 342 */ \
  OP(GetTexParameteriv)                                        /* 343 */ \
  OP(GetUniformfv)                                             /* 344 */ \
  OP(GetUniformiv)                                             /* 345 */ \
  OP(GetUniformLocation)                                       /* 346 */ \
  OP(GetUniformLocationImmediate)                              /* 347 */ \
  OP(GetVertexAttribfv)                                        /* 348 */ \
  OP(GetVertexAttribiv)                                        /* 349 */ \
  OP(GetVertexAttribPointerv)                                  /* 350 */ \
  OP(Hint)                                                     /* 351 */ \
  OP(IsBuffer)                                                 /* 352 */ \
  OP(IsEnabled)                                                /* 353 */ \
  OP(IsFramebuffer)                                            /* 354 */ \
  OP(IsProgram)                                                /* 355 */ \
  OP(IsRenderbuffer)                                           /* 356 */ \
  OP(IsShader)                                                 /* 357 */ \
  OP(IsTexture)                                                /* 358 */ \
  OP(LineWidth)                                                /* 359 */ \
  OP(LinkProgram)                                              /* 360 */ \
  OP(PixelStorei)                                              /* 361 */ \
  OP(PolygonOffset)                                            /* 362 */ \
  OP(ReadPixels)                                               /* 363 */ \
  OP(RenderbufferStorage)                                      /* 364 */ \
  OP(SampleCoverage)                                           /* 365 */ \
  OP(Scissor)                                                  /* 366 */ \
  OP(ShaderSource)                                             /* 367 */ \
  OP(ShaderSourceImmediate)                                    /* 368 */ \
  OP(StencilFunc)                                              /* 369 */ \
  OP(StencilFuncSeparate)                                      /* 370 */ \
  OP(StencilMask)                                              /* 371 */ \
  OP(StencilMaskSeparate)                                      /* 372 */ \
  OP(StencilOp)                                                /* 373 */ \
  OP(StencilOpSeparate)                                        /* 374 */ \
  OP(TexImage2D)                                               /* 375 */ \
  OP(TexImage2DImmediate)                                      /* 376 */ \
  OP(TexParameterf)                                            /* 377 */ \
  OP(TexParameterfv)                                           /* 378 */ \
  OP(TexParameterfvImmediate)                                  /* 379 */ \
  OP(TexParameteri)                                            /* 380 */ \
  OP(TexParameteriv)                                           /* 381 */ \
  OP(TexParameterivImmediate)                                  /* 382 */ \
  OP(TexSubImage2D)                                            /* 383 */ \
  OP(TexSubImage2DImmediate)                                   /* 384 */ \
  OP(Uniform1f)                                                /* 385 */ \
  OP(Uniform1fv)                                               /* 386 */ \
  OP(Uniform1fvImmediate)                                      /* 387 */ \
  OP(Uniform1i)                                                /* 388 */ \
  OP(Uniform1iv)                                               /* 389 */ \
  OP(Uniform1ivImmediate)                                      /* 390 */ \
  OP(Uniform2f)                                                /* 391 */ \
  OP(Uniform2fv)                                               /* 392 */ \
  OP(Uniform2fvImmediate)                                      /* 393 */ \
  OP(Uniform2i)                                                /* 394 */ \
  OP(Uniform2iv)                                               /* 395 */ \
  OP(Uniform2ivImmediate)                                      /* 396 */ \
  OP(Uniform3f)                                                /* 397 */ \
  OP(Uniform3fv)                                               /* 398 */ \
  OP(Uniform3fvImmediate)                                      /* 399 */ \
  OP(Uniform3i)                                                /* 400 */ \
  OP(Uniform3iv)                                               /* 401 */ \
  OP(Uniform3ivImmediate)                                      /* 402 */ \
  OP(Uniform4f)                                                /* 403 */ \
  OP(Uniform4fv)                                               /* 404 */ \
  OP(Uniform4fvImmediate)                                      /* 405 */ \
  OP(Uniform4i)                                                /* 406 */ \
  OP(Uniform4iv)                                               /* 407 */ \
  OP(Uniform4ivImmediate)                                      /* 408 */ \
  OP(UniformMatrix2fv)                                         /* 409 */ \
  OP(UniformMatrix2fvImmediate)                                /* 410 */ \
  OP(UniformMatrix3fv)                                         /* 411 */ \
  OP(UniformMatrix3fvImmediate)                                /* 412 */ \
  OP(UniformMatrix4fv)                                         /* 413 */ \
  OP(UniformMatrix4fvImmediate)                                /* 414 */ \
  OP(UseProgram)                                               /* 415 */ \
  OP(ValidateProgram)                                          /* 416 */ \
  OP(VertexAttrib1f)                                           /* 417 */ \
  OP(VertexAttrib1fv)                                          /* 418 */ \
  OP(VertexAttrib1fvImmediate)                                 /* 419 */ \
  OP(VertexAttrib2f)                                           /* 420 */ \
  OP(VertexAttrib2fv)                                          /* 421 */ \
  OP(VertexAttrib2fvImmediate)                                 /* 422 */ \
  OP(VertexAttrib3f)                                           /* 423 */ \
  OP(VertexAttrib3fv)                                          /* 424 */ \
  OP(VertexAttrib3fvImmediate)                                 /* 425 */ \
  OP(VertexAttrib4f)                                           /* 426 */ \
  OP(VertexAttrib4fv)                                          /* 427 */ \
  OP(VertexAttrib4fvImmediate)                                 /* 428 */ \
  OP(VertexAttribPointer)                                      /* 429 */ \
  OP(Viewport)                                                 /* 430 */ \
  OP(SwapBuffers)                                              /* 431 */ \
  OP(BindAttribLocationBucket)                                 /* 432 */ \
  OP(GetUniformLocationBucket)                                 /* 433 */ \
  OP(GetAttribLocationBucket)                                  /* 434 */ \
  OP(ShaderSourceBucket)                                       /* 435 */ \
  OP(ShaderBinary)                                             /* 436 */ \
  OP(ReleaseShaderCompiler)                                    /* 437 */ \
  OP(GetMaxValueInBufferCHROMIUM)                              /* 438 */ \
  OP(GenSharedIdsCHROMIUM)                                     /* 439 */ \
  OP(DeleteSharedIdsCHROMIUM)                                  /* 440 */ \
  OP(RegisterSharedIdsCHROMIUM)                                /* 441 */ \
  OP(EnableFeatureCHROMIUM)                                    /* 442 */ \
  OP(CompressedTexImage2DBucket)                               /* 443 */ \
  OP(CompressedTexSubImage2DBucket)                            /* 444 */ \
  OP(RenderbufferStorageMultisampleEXT)                        /* 445 */ \
  OP(BlitFramebufferEXT)                                       /* 446 */ \
  OP(Placeholder447CHROMIUM)                                   /* 447 */ \
  OP(ResizeCHROMIUM)                                           /* 448 */ \
  OP(GetRequestableExtensionsCHROMIUM)                         /* 449 */ \
  OP(RequestExtensionCHROMIUM)                                 /* 450 */ \
  OP(CreateStreamTextureCHROMIUM)                              /* 451 */ \
  OP(DestroyStreamTextureCHROMIUM)                             /* 452 */ \
  OP(Placeholder453CHROMIUM)                                   /* 453 */ \
  OP(GetMultipleIntegervCHROMIUM)                              /* 454 */ \
  OP(GetProgramInfoCHROMIUM)                                   /* 455 */ \
  OP(GetTranslatedShaderSourceANGLE)                           /* 456 */ \

enum CommandId {
  kStartPoint = cmd::kLastCommonId,  // All GLES2 commands start after this.
#define GLES2_CMD_OP(name) k ## name,
  GLES2_COMMAND_LIST(GLES2_CMD_OP)
#undef GLES2_CMD_OP
  kNumCommands
};

#endif  // GPU_COMMAND_BUFFER_COMMON_GLES2_CMD_IDS_AUTOGEN_H_

