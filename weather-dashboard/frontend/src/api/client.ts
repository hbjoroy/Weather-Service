import axios from 'axios'
import type { AxiosInstance, AxiosError } from 'axios'
import type { 
  UserProfile, 
  WeatherResponse, 
  ForecastResponse, 
  ForecastRequest,
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

  // Profile management
  async getProfile(): Promise<UserProfile> {
    const response = await this.client.get<UserProfile>('/profile')
    return response.data
  }

  async updateProfile(profile: UserProfile): Promise<UserProfile> {
    const response = await this.client.put<UserProfile>('/profile', profile)
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