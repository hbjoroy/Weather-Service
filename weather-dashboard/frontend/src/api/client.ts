import axios from 'axios'
import type { AxiosInstance, AxiosError } from 'axios'
import type { 
  UserProfile, 
  WeatherResponse, 
  ForecastResponse, 
  ForecastRequest,
  LoginRequest,
  LoginResponse,
  ErrorResponse 
} from '@/types/weather'

class WeatherDashboardAPI {
  private client: AxiosInstance

  constructor(baseURL: string = '/api') {
    this.client = axios.create({
      baseURL,
      headers: {
        'Content-Type': 'application/json; charset=utf-8',
      },
      timeout: 30000,
      withCredentials: true, // Enable cookies for session management
    })

    // Response interceptor for error handling
    this.client.interceptors.response.use(
      (response) => response,
      (error: AxiosError<ErrorResponse>) => {
        if (error.response?.data?.error) {
          const apiError = error.response.data.error
          throw new Error(`${apiError.message} (Code: ${apiError.code})`)
        }
        throw error
      }
    )
  }

  // Authentication
  async login(request: LoginRequest, useLegacy: boolean = false): Promise<LoginResponse> {
    // If legacy login requested, or if we have credentials, use legacy method
    if (useLegacy || (request.userId && request.name)) {
      const response = await this.client.post<LoginResponse>('/login', request)
      return response.data
    }

    // Try OIDC login (modern method)
    try {
      const response = await this.client.get<{ redirectUrl: string }>('/auth/login')
      // Redirect to the OIDC provider
      window.location.href = response.data.redirectUrl
      // Return a pending response (will never actually return as we're redirecting)
      return { success: true, userId: '', name: '', sessionId: '' }
    } catch (oidcError) {
      console.error('OIDC login failed:', oidcError)
      throw oidcError
    }
  }

  async logout(): Promise<void> {
    await this.client.post('/logout')
  }

  // Profile management
  async getProfile(): Promise<UserProfile> {
    const response = await this.client.get<UserProfile>('/profile')
    return response.data
  }

  async updateProfile(profile: Partial<UserProfile>): Promise<UserProfile> {
    const response = await this.client.post<UserProfile>('/profile', profile)
    return response.data
  }

  // Weather data
  async getCurrentWeather(location: string, includeAqi: boolean = false): Promise<WeatherResponse> {
    const response = await this.client.get<WeatherResponse>('/weather/current', {
      params: {
        location,
        include_aqi: includeAqi
      }
    })
    return response.data
  }

  async getForecast(request: ForecastRequest): Promise<ForecastResponse> {
    const response = await this.client.get<ForecastResponse>('/weather/forecast', {
      params: {
        location: request.location,
        days: request.days,
        include_aqi: request.include_aqi || false,
        include_alerts: request.include_alerts || false,
        include_hourly: request.include_hourly || false
      }
    })
    return response.data
  }
}

// Export singleton instance
export const api = new WeatherDashboardAPI()
export default api